#include "ForegroundWatcher.h"

#include <algorithm>
#include <cwctype>

namespace {

constexpr UINT_PTR kPollTimerId = 1;
constexpr UINT kPollIntervalMs = 200;
// 离开游戏时的确认窗口：切换动画、登录器弹窗等造成的短暂失焦不算。
constexpr DWORD kLeaveDebounceMs = 250;

std::wstring ToLowerWide(std::wstring text) {
    std::transform(text.begin(), text.end(), text.begin(),
        [](wchar_t ch) { return static_cast<wchar_t>(towlower(ch)); });
    return text;
}

std::wstring ExtractFileName(const std::wstring& path) {
    const auto pos = path.find_last_of(L"\\/");
    return pos == std::wstring::npos ? path : path.substr(pos + 1);
}

}  // namespace

std::wstring QueryForegroundProcessName(HWND foregroundWindow) {
    DWORD processId = 0;
    if (foregroundWindow == nullptr ||
        ::GetWindowThreadProcessId(foregroundWindow, &processId) == 0 || processId == 0) {
        return {};
    }

    HANDLE process = ::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId);
    if (process == nullptr) {
        return {};
    }

    wchar_t buffer[MAX_PATH] = {};
    DWORD size = MAX_PATH;
    std::wstring name;
    if (::QueryFullProcessImageNameW(process, 0, buffer, &size) != FALSE) {
        name = ToLowerWide(ExtractFileName(std::wstring(buffer, size)));
    }
    ::CloseHandle(process);
    return name;
}

bool ForegroundWatcher::MatchesBuiltInProcessName(const std::wstring& lowerExeName) {
    return lowerExeName.rfind(L"jx3client", 0) == 0;
}

bool ForegroundWatcher::MatchesCustomProcess(
    const std::wstring& lowerExeName, const std::vector<std::string>& customProcesses) {
    for (const auto& custom : customProcesses) {
        if (custom.empty()) continue;
        std::wstring pattern(custom.begin(), custom.end());
        pattern = ToLowerWide(pattern);
        if (!pattern.empty() && pattern.back() == L'*') {
            const std::wstring prefix = pattern.substr(0, pattern.size() - 1);
            if (!prefix.empty() && lowerExeName.rfind(prefix, 0) == 0) {
                return true;
            }
            continue;
        }
        if (lowerExeName == pattern) {
            return true;
        }
    }
    return false;
}

ForegroundWatcher::~ForegroundWatcher() {
    Stop();
}

bool ForegroundWatcher::Start() {
    if (thread_.joinable()) {
        return true;
    }
    exit_.store(false);
    gameForeground_.store(false);
    gameHwnd_.store(nullptr);
    pendingLeaveTick_ = 0;
    thread_ = std::thread(&ForegroundWatcher::ThreadMain, this);
    return true;
}

void ForegroundWatcher::Stop() {
    exit_.store(true);
    if (thread_.joinable()) {
        thread_.join();
    }
}

void ForegroundWatcher::SetRules(Rules rules) {
    std::lock_guard lock(rulesMutex_);
    rules_ = std::move(rules);
}

void ForegroundWatcher::SetCallback(ChangeCallback callback) {
    std::lock_guard lock(callbackMutex_);
    callback_ = std::move(callback);
}

bool ForegroundWatcher::IsGameForeground() const noexcept {
    return gameForeground_.load();
}

bool ForegroundWatcher::TryGetGameClientRect(RECT& outRect) const {
    const HWND window = gameHwnd_.load();
    if (window == nullptr || !::IsWindow(window)) {
        return false;
    }

    RECT client{};
    if (!::GetClientRect(window, &client)) {
        return false;
    }
    POINT topLeft{0, 0};
    if (!::ClientToScreen(window, &topLeft)) {
        return false;
    }

    outRect.left = topLeft.x;
    outRect.top = topLeft.y;
    outRect.right = topLeft.x + client.right;
    outRect.bottom = topLeft.y + client.bottom;
    return true;
}

void ForegroundWatcher::ThreadMain() {
    // 轮询式检测：200ms 一次，足够门控使用，且不依赖 WinEventHook 的静态回填。
    ::SetTimer(nullptr, kPollTimerId, kPollIntervalMs, nullptr);

    MSG message{};
    while (!exit_.load()) {
        const BOOL hasMessage = ::GetMessageW(&message, nullptr, 0, 0);
        if (hasMessage <= 0) {
            break;
        }
        if (message.message == WM_TIMER && message.wParam == kPollTimerId) {
            EvaluateForeground();
            continue;
        }
        ::TranslateMessage(&message);
        ::DispatchMessageW(&message);
    }

    ::KillTimer(nullptr, kPollTimerId);
}

void ForegroundWatcher::EvaluateForeground() {
    const HWND foreground = ::GetForegroundWindow();
    if (foreground == nullptr) {
        return;
    }

    const std::wstring processName = QueryForegroundProcessName(foreground);

    Rules rules;
    {
        std::lock_guard lock(rulesMutex_);
        rules = rules_;
    }

    const bool matched = MatchesBuiltInProcessName(processName)
        || MatchesCustomProcess(processName, rules.customProcesses);
    if (matched) {
        pendingLeaveTick_ = 0;
        SetResult(true, foreground);
        return;
    }

    if (gameForeground_.load()) {
        const DWORD now = ::GetTickCount();
        if (pendingLeaveTick_ == 0) {
            pendingLeaveTick_ = now;
            return;
        }
        if (now - pendingLeaveTick_ < kLeaveDebounceMs) {
            return;
        }
    }
    pendingLeaveTick_ = 0;
    SetResult(false, nullptr);
}

void ForegroundWatcher::SetResult(bool gameForeground, HWND gameWindow) {
    // 即使前台状态没变，也要跟踪最新的游戏窗口句柄（多开切换时锚点跟随前台）。
    gameHwnd_.store(gameWindow);
    const bool changed = gameForeground_.load() != gameForeground;
    gameForeground_.store(gameForeground);
    if (!changed) {
        return;
    }

    ChangeCallback callback;
    {
        std::lock_guard lock(callbackMutex_);
        callback = callback_;
    }
    if (callback) {
        callback(gameForeground);
    }
}
