#include "ForegroundWatcher.h"

#include <algorithm>
#include <cwctype>

namespace {

constexpr UINT kPollIntervalMs = 200;
// 离开游戏时的确认窗口：切换动画、登录器弹窗等造成的短暂失焦不算。
// 前台变化已由 WinEvent 事件即时通知，这里只容忍极短的焦点抖动。
constexpr DWORD kLeaveDebounceMs = 100;

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
    lastForegroundWindow_ = nullptr;
    pendingLeaveTick_ = 0;
    thread_ = std::thread(&ForegroundWatcher::ThreadMain, this);
    return true;
}

void ForegroundWatcher::Stop() {
    exit_.store(true);
    if (threadId_ != 0) {
        // 唤醒可能阻塞在 GetMessage 上的 watcher 线程，让循环立刻检查退出标志。
        ::PostThreadMessageW(threadId_, WM_NULL, 0, 0);
    }
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

void ForegroundWatcher::SetWindowChangeCallback(WindowChangeCallback callback) {
    std::lock_guard lock(callbackMutex_);
    windowChangeCallback_ = std::move(callback);
}

bool ForegroundWatcher::IsGameForeground() const noexcept {
    return gameForeground_.load();
}

ForegroundWatcher* ForegroundWatcher::activeInstance_ = nullptr;

void ForegroundWatcher::ThreadMain() {
    threadId_ = ::GetCurrentThreadId();
    activeInstance_ = this;

    // 前台切换即时感知：事件回调在本线程的消息循环里分发，切换瞬间即完成判定。
    // 进游戏零延迟恢复；离开游戏只留 kLeaveDebounceMs 的焦点抖动容忍。
    winEventHook_ = ::SetWinEventHook(
        EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND, nullptr,
        &ForegroundWatcher::WinEventProc, 0, 0, WINEVENT_OUTOFCONTEXT);

    // 轮询只作为安全网：事件钩子安装失败或个别全屏切换漏发事件时兜底。
    // 注意：hwnd 为 NULL 时 Windows 会忽略传入的定时器 ID、自行分配新 ID，
    // 匹配 WM_TIMER 和 KillTimer 都必须用 SetTimer 的返回值。
    pollTimerId_ = ::SetTimer(nullptr, 0, kPollIntervalMs, nullptr);
    debounceTimerId_ = 0;

    MSG message{};
    while (!exit_.load()) {
        const BOOL hasMessage = ::GetMessageW(&message, nullptr, 0, 0);
        if (hasMessage <= 0) {
            break;
        }
        if (pollTimerId_ != 0 && message.message == WM_TIMER && message.wParam == pollTimerId_) {
            EvaluateForeground();
            continue;
        }
        if (debounceTimerId_ != 0 && message.message == WM_TIMER && message.wParam == debounceTimerId_) {
            CancelLeaveDebounceTimer();
            EvaluateForeground();
            continue;
        }
        ::TranslateMessage(&message);
        ::DispatchMessageW(&message);
    }

    CancelLeaveDebounceTimer();
    if (pollTimerId_ != 0) {
        ::KillTimer(nullptr, pollTimerId_);
        pollTimerId_ = 0;
    }
    if (winEventHook_ != nullptr) {
        ::UnhookWinEvent(winEventHook_);
        winEventHook_ = nullptr;
    }
    activeInstance_ = nullptr;
    threadId_ = 0;
}

void CALLBACK ForegroundWatcher::WinEventProc(HWINEVENTHOOK, DWORD, HWND, LONG, LONG, DWORD, DWORD) {
    if (activeInstance_ != nullptr) {
        activeInstance_->EvaluateForeground();
    }
}

void ForegroundWatcher::EvaluateForeground() {
    const HWND foreground = ::GetForegroundWindow();
    if (foreground == nullptr) {
        return;
    }

    if (foreground != lastForegroundWindow_) {
        lastForegroundWindow_ = foreground;
        WindowChangeCallback callback;
        {
            std::lock_guard lock(callbackMutex_);
            callback = windowChangeCallback_;
        }
        if (callback) callback(foreground);
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
        CancelLeaveDebounceTimer();
        SetResult(true);
        return;
    }

    if (gameForeground_.load()) {
        const DWORD now = ::GetTickCount();
        if (pendingLeaveTick_ == 0) {
            pendingLeaveTick_ = now;
            // 到点即复查：一次性精确定时器，不依赖安全网轮询的间隔。
            ArmLeaveDebounceTimer();
            return;
        }
        if (now - pendingLeaveTick_ < kLeaveDebounceMs) {
            return;
        }
    }
    pendingLeaveTick_ = 0;
    SetResult(false);
}

void ForegroundWatcher::ArmLeaveDebounceTimer() {
    if (debounceTimerId_ != 0) {
        return;
    }
    debounceTimerId_ = ::SetTimer(nullptr, 0, kLeaveDebounceMs + 20, nullptr);
}

void ForegroundWatcher::CancelLeaveDebounceTimer() {
    if (debounceTimerId_ != 0) {
        ::KillTimer(nullptr, debounceTimerId_);
        debounceTimerId_ = 0;
    }
}

void ForegroundWatcher::SetResult(bool gameForeground) {
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
