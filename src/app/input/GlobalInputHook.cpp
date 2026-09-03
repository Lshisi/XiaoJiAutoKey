#include "input/GlobalInputHook.h"

#include "input/InputKeyUtils.h"

#include <QCoreApplication>
#include <QMetaObject>
#include <stdexcept>
#include <utility>
#include <windowsx.h>

namespace {

constexpr int kStartupIgnoreWindowMs = 300;

GlobalInputHook* g_hookInstance = nullptr;
const QEvent::Type kControlHotkeyEventType =
    static_cast<QEvent::Type>(QEvent::registerEventType());

void postControlHotkeyEvent(GlobalInputHook* hook) {
    if (!hook) return;
    QCoreApplication::postEvent(hook, new QEvent(kControlHotkeyEventType), Qt::HighEventPriority);
}

}  // namespace

GlobalInputHook::GlobalInputHook(QObject* parent) : QObject(parent) {}

GlobalInputHook::~GlobalInputHook() {
    stop();
}

bool GlobalInputHook::start() {
    if (hookThread_.joinable()) {
        // A failed hook initialization can leave a finished but still joinable
        // thread behind. Join it before attempting a real restart.
        if (hookThreadId_.load() != 0) return true;
        hookThread_.join();
    }

    {
        std::lock_guard lock(hookInitMutex_);
        hookInitCompleted_ = false;
        hookInitSucceeded_ = false;
    }

    g_hookInstance = this;
    // 只保留很短的启动静默窗，避免把用户首个启停热键也吞掉。
    startupIgnoreUntilTick_ = GetTickCount64() + kStartupIgnoreWindowMs;
    hookThread_ = std::thread(&GlobalInputHook::runHookLoop, this);

    std::unique_lock lock(hookInitMutex_);
    hookInitCv_.wait(lock, [&] { return hookInitCompleted_; });
    return hookInitSucceeded_;
}

void GlobalInputHook::stop() {
    if (hookThreadId_ != 0) {
        PostThreadMessageW(hookThreadId_, WM_QUIT, 0, 0);
    }
    if (hookThread_.joinable()) {
        hookThread_.join();
    }
    if (g_hookInstance == this) {
        g_hookInstance = nullptr;
    }
    controlHotkeyPressed_ = false;
}

bool GlobalInputHook::isRunning() const noexcept {
    return hookThreadId_.load() != 0;
}

void GlobalInputHook::setControlHotkey(std::optional<KeyDescriptor> key) {
    std::lock_guard lock(stateMutex_);
    controlHotkey_ = key;
    controlHotkeyPressed_ = false;
}

void GlobalInputHook::setCaptureActive(bool active) {
    std::lock_guard lock(stateMutex_);
    captureActive_ = active;
    if (!captureActive_) captureStartedTick_ = 0;
}

void GlobalInputHook::setCaptureStartedTick(ULONGLONG tick) {
    std::lock_guard lock(stateMutex_);
    captureStartedTick_ = tick;
}

void GlobalInputHook::setDialogOpen(bool open) {
    dialogOpen_ = open;
}

void GlobalInputHook::setRepeatRunning(bool running) {
    repeatRunning_ = running;
}

void GlobalInputHook::setRawKeyboardActive(bool active) {
    if (rawKeyboardActive_.exchange(active) != active) {
        controlHotkeyPressed_ = false;
    }
}

void GlobalInputHook::setRawMouseActive(bool active) {
    if (rawMouseActive_.exchange(active) != active) {
        controlHotkeyPressed_ = false;
    }
}

void GlobalInputHook::setSelfInjectedFilter(std::function<bool(const KeyDescriptor&, bool)> filter) {
    std::lock_guard lock(stateMutex_);
    selfInjectedFilter_ = std::move(filter);
}

bool GlobalInputHook::isDialogOpen() const noexcept {
    return dialogOpen_.load();
}

bool GlobalInputHook::isRawKeyboardActive() const noexcept {
    return rawKeyboardActive_.load();
}

bool GlobalInputHook::isRawMouseActive() const noexcept {
    return rawMouseActive_.load();
}

bool GlobalInputHook::event(QEvent* event) {
    if (event && event->type() == kControlHotkeyEventType) {
        emit controlHotkeyPressed();
        return true;
    }
    return QObject::event(event);
}

void GlobalInputHook::runHookLoop() {
    MSG msg;
    PeekMessageW(&msg, nullptr, WM_USER, WM_USER, PM_NOREMOVE);
    hookThreadId_ = GetCurrentThreadId();
    keyboardHook_ = SetWindowsHookExW(WH_KEYBOARD_LL, &GlobalInputHook::KeyboardHookProc, nullptr, 0);
    mouseHook_ = SetWindowsHookExW(WH_MOUSE_LL, &GlobalInputHook::MouseHookProc, nullptr, 0);
    {
        std::lock_guard lock(hookInitMutex_);
        hookInitSucceeded_ = (keyboardHook_ && mouseHook_);
        hookInitCompleted_ = true;
    }
    hookInitCv_.notify_one();

    if (!hookInitSucceeded_) {
        if (keyboardHook_) { UnhookWindowsHookEx(keyboardHook_); keyboardHook_ = nullptr; }
        if (mouseHook_) { UnhookWindowsHookEx(mouseHook_); mouseHook_ = nullptr; }
        hookThreadId_ = 0;
        return;
    }

    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    if (keyboardHook_) { UnhookWindowsHookEx(keyboardHook_); keyboardHook_ = nullptr; }
    if (mouseHook_) { UnhookWindowsHookEx(mouseHook_); mouseHook_ = nullptr; }
    hookThreadId_ = 0;
}

LRESULT CALLBACK GlobalInputHook::KeyboardHookProc(int code, WPARAM wParam, LPARAM lParam) {
    if (code < 0 || !g_hookInstance) return CallNextHookEx(nullptr, code, wParam, lParam);
    const auto* info = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);
    const KeyDescriptor key = KeyboardHookKeyDescriptor(info->vkCode, info->scanCode, info->flags);
    const bool pressed = wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN;
    const bool released = wParam == WM_KEYUP || wParam == WM_SYSKEYUP;
    if (!pressed && !released) return CallNextHookEx(nullptr, code, wParam, lParam);
    if (info->flags & LLKHF_INJECTED) return CallNextHookEx(nullptr, code, wParam, lParam);

    auto* hook = g_hookInstance;
    const bool repeatRunning = hook->repeatRunning_.load();
    std::optional<KeyDescriptor> controlHotkey;
    std::function<bool(const KeyDescriptor&, bool)> selfInjectedFilter;
    bool captureActive = false;
    {
        std::lock_guard lock(hook->stateMutex_);
        controlHotkey = hook->controlHotkey_;
        selfInjectedFilter = hook->selfInjectedFilter_;
        captureActive = hook->captureActive_;
    }

    if (captureActive && pressed) {
        const auto encoded = EncodeKey(key);
        QMetaObject::invokeMethod(hook, [hook, encoded] { emit hook->capturedKey(encoded); }, Qt::QueuedConnection);
        return 1;
    }

    if (hook->dialogOpen_.load()) return CallNextHookEx(nullptr, code, wParam, lParam);

    // The control hotkey remains on the low-level hook even while Raw Input
    // is enabled, so a stale Raw Input registration cannot disable toggling.
    const bool isControlHotkey = controlHotkey.has_value() && *controlHotkey == key;
    if (isControlHotkey) {
        if (pressed && !hook->controlHotkeyPressed_.exchange(true)) {
            postControlHotkeyEvent(hook);
        } else if (released) {
            hook->controlHotkeyPressed_ = false;
        }
        QMetaObject::invokeMethod(hook, [hook, key, pressed] {
            emit hook->macroTriggerChanged(key, pressed);
        }, Qt::QueuedConnection);
        return CallNextHookEx(nullptr, code, wParam, lParam);
    }

    if (selfInjectedFilter && selfInjectedFilter(key, pressed)) {
        return CallNextHookEx(nullptr, code, wParam, lParam);
    }

    const ULONGLONG nowTick = GetTickCount64();
    const ULONGLONG ignoreUntil = hook->startupIgnoreUntilTick_.load();
    if (nowTick < ignoreUntil) {
        return CallNextHookEx(nullptr, code, wParam, lParam);
    }

    if (!captureActive && hook->rawKeyboardActive_.load()) {
        return CallNextHookEx(nullptr, code, wParam, lParam);
    }

    if (repeatRunning) {
        QMetaObject::invokeMethod(hook, [hook, key, pressed] {
            emit hook->physicalKeyChanged(key, pressed);
        }, Qt::QueuedConnection);
    }
    QMetaObject::invokeMethod(hook, [hook, key, pressed] {
        emit hook->macroTriggerChanged(key, pressed);
    }, Qt::QueuedConnection);

    return CallNextHookEx(nullptr, code, wParam, lParam);
}

LRESULT CALLBACK GlobalInputHook::MouseHookProc(int code, WPARAM wParam, LPARAM lParam) {
    if (code < 0 || !g_hookInstance) return CallNextHookEx(nullptr, code, wParam, lParam);
    const auto* info = reinterpret_cast<MSLLHOOKSTRUCT*>(lParam);
    if (!IsMouseDownMessage(wParam) && !IsMouseUpMessage(wParam)) return CallNextHookEx(nullptr, code, wParam, lParam);
    const KeyDescriptor key = MouseMessageToDescriptor(wParam, *info);
    if (!key.IsValid()) return CallNextHookEx(nullptr, code, wParam, lParam);
    const bool pressed = IsMouseDownMessage(wParam);

    auto* hook = g_hookInstance;
    std::optional<KeyDescriptor> controlHotkey;
    std::function<bool(const KeyDescriptor&, bool)> selfInjectedFilter;
    bool captureActive = false;
    ULONGLONG captureStartedTick = 0;
    {
        std::lock_guard lock(hook->stateMutex_);
        controlHotkey = hook->controlHotkey_;
        selfInjectedFilter = hook->selfInjectedFilter_;
        captureActive = hook->captureActive_;
        captureStartedTick = hook->captureStartedTick_;
    }

    if (info->flags & LLMHF_INJECTED) return CallNextHookEx(nullptr, code, wParam, lParam);

    const bool repeatRunning = hook->repeatRunning_.load();

    if (captureActive && pressed) {
        if (GetTickCount64() - captureStartedTick < 150) return CallNextHookEx(nullptr, code, wParam, lParam);
        const auto encoded = EncodeKey(key);
        QMetaObject::invokeMethod(hook, [hook, encoded] { emit hook->capturedKey(encoded); }, Qt::QueuedConnection);
        return 1;
    }

    if (hook->dialogOpen_.load()) return CallNextHookEx(nullptr, code, wParam, lParam);

    const bool isControlHotkey = controlHotkey.has_value() && *controlHotkey == key;
    if (isControlHotkey) {
        if (pressed && !hook->controlHotkeyPressed_.exchange(true)) {
            postControlHotkeyEvent(hook);
        } else if (!pressed) {
            hook->controlHotkeyPressed_ = false;
        }
        QMetaObject::invokeMethod(hook, [hook, key, pressed] {
            emit hook->macroTriggerChanged(key, pressed);
        }, Qt::QueuedConnection);
        return CallNextHookEx(nullptr, code, wParam, lParam);
    }

    if (selfInjectedFilter && selfInjectedFilter(key, pressed)) {
        return CallNextHookEx(nullptr, code, wParam, lParam);
    }

    const ULONGLONG nowTick = GetTickCount64();
    const ULONGLONG ignoreUntil = hook->startupIgnoreUntilTick_.load();
    if (nowTick < ignoreUntil) return CallNextHookEx(nullptr, code, wParam, lParam);

    if (!captureActive && hook->rawMouseActive_.load()) {
        return CallNextHookEx(nullptr, code, wParam, lParam);
    }

    if (repeatRunning) {
        QMetaObject::invokeMethod(hook, [hook, key, pressed] {
            emit hook->physicalKeyChanged(key, pressed);
        }, Qt::QueuedConnection);
    }
    QMetaObject::invokeMethod(hook, [hook, key, pressed] {
        emit hook->macroTriggerChanged(key, pressed);
    }, Qt::QueuedConnection);

    return CallNextHookEx(nullptr, code, wParam, lParam);
}
