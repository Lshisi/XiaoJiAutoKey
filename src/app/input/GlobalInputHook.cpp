#include "input/GlobalInputHook.h"

#include "input/InputKeyUtils.h"

#include <QCoreApplication>
#include <QMetaObject>
#include <stdexcept>
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
    if (hookThread_.joinable()) return true;

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
    if (!running) controlHotkeyPressed_ = false;
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
    const ULONGLONG nowTick = GetTickCount64();
    const ULONGLONG ignoreUntil = hook->startupIgnoreUntilTick_.load();
    if (nowTick < ignoreUntil) {
        return CallNextHookEx(nullptr, code, wParam, lParam);
    }

    const bool repeatRunning = hook->repeatRunning_.load();
    std::optional<KeyDescriptor> controlHotkey;
    bool captureActive = false;
    {
        std::lock_guard lock(hook->stateMutex_);
        controlHotkey = hook->controlHotkey_;
        captureActive = hook->captureActive_;
    }

    if (captureActive && pressed) {
        const auto encoded = EncodeKey(key);
        QMetaObject::invokeMethod(hook, [hook, encoded] { emit hook->capturedKey(encoded); }, Qt::QueuedConnection);
        return 1;
    }

    if (hook->dialogOpen_.load()) return CallNextHookEx(nullptr, code, wParam, lParam);

    const bool isControlHotkey = controlHotkey.has_value() && *controlHotkey == key;
    if (isControlHotkey) {
        if (pressed && !hook->controlHotkeyPressed_.exchange(true)) {
            postControlHotkeyEvent(hook);
        } else if (released) {
            hook->controlHotkeyPressed_ = false;
        }
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
    if (info->flags & LLMHF_INJECTED) return CallNextHookEx(nullptr, code, wParam, lParam);

    auto* hook = g_hookInstance;
    const ULONGLONG nowTick = GetTickCount64();
    const ULONGLONG ignoreUntil = hook->startupIgnoreUntilTick_.load();
    if (nowTick < ignoreUntil) return CallNextHookEx(nullptr, code, wParam, lParam);

    const bool pressed = IsMouseDownMessage(wParam);
    const bool repeatRunning = hook->repeatRunning_.load();
    std::optional<KeyDescriptor> controlHotkey;
    bool captureActive = false;
    ULONGLONG captureStartedTick = 0;
    {
        std::lock_guard lock(hook->stateMutex_);
        controlHotkey = hook->controlHotkey_;
        captureActive = hook->captureActive_;
        captureStartedTick = hook->captureStartedTick_;
    }

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
