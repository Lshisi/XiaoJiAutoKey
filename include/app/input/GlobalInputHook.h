#pragma once

#include "AppTypes.h"

#include <QObject>
#include <QEvent>
#include <Windows.h>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <thread>

class GlobalInputHook : public QObject {
    Q_OBJECT

public:
    explicit GlobalInputHook(QObject* parent = nullptr);
    ~GlobalInputHook() override;

    bool start();
    void stop();

    void setControlHotkey(std::optional<KeyDescriptor> key);
    void setCaptureActive(bool active);
    void setCaptureStartedTick(ULONGLONG tick);
    void setDialogOpen(bool open);
    void setRepeatRunning(bool running);
    void setRawKeyboardActive(bool active);

    [[nodiscard]] bool isDialogOpen() const noexcept;
    [[nodiscard]] bool isRawKeyboardActive() const noexcept;

signals:
    void capturedKey(quint64 encodedKey);
    void controlHotkeyPressed();
    void physicalKeyChanged(KeyDescriptor key, bool pressed);
    void macroTriggerChanged(KeyDescriptor key, bool pressed);

protected:
    bool event(QEvent* event) override;

private:
    void runHookLoop();

    static LRESULT CALLBACK KeyboardHookProc(int code, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK MouseHookProc(int code, WPARAM wParam, LPARAM lParam);

    HHOOK keyboardHook_ = nullptr;
    HHOOK mouseHook_ = nullptr;

    std::thread hookThread_;
    std::atomic<DWORD> hookThreadId_{0};
    std::mutex hookInitMutex_;
    std::condition_variable hookInitCv_;
    bool hookInitCompleted_ = false;
    bool hookInitSucceeded_ = false;

    mutable std::mutex stateMutex_;
    std::optional<KeyDescriptor> controlHotkey_;
    bool captureActive_ = false;
    ULONGLONG captureStartedTick_ = 0;

    std::atomic<bool> dialogOpen_{false};
    std::atomic<bool> repeatRunning_{false};
    std::atomic<bool> rawKeyboardActive_{false};
    std::atomic<bool> controlHotkeyPressed_{false};
    std::atomic<ULONGLONG> startupIgnoreUntilTick_{0};
};
