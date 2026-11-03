#pragma once

#include "AppTypes.h"
#include "MacroRecording.h"

#include <QObject>
#include <QEvent>
#include <Windows.h>
#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <optional>
#include <thread>

class GlobalInputHook : public QObject {
    Q_OBJECT

public:
    explicit GlobalInputHook(QObject* parent = nullptr);
    ~GlobalInputHook() override;

    void setRecordingActive(bool active);
    [[nodiscard]] bool isRecordingActive() const { return recordingActive_.load(); }
    bool start();
    void stop();

    void setControlHotkey(std::optional<KeyDescriptor> key);
    void setCaptureActive(bool active);
    void setCaptureStartedTick(ULONGLONG tick);
    void setDialogOpen(bool open);
    void setRepeatRunning(bool running);
    void setRawKeyboardActive(bool active);
    void setRawMouseActive(bool active);
    void setSelfInjectedFilter(std::function<bool(const KeyDescriptor&, bool)> filter);
    void setPhysicalInputFilter(std::function<bool(const KeyDescriptor&)> filter);

    [[nodiscard]] bool isDialogOpen() const noexcept;
    [[nodiscard]] bool isRawKeyboardActive() const noexcept;
    [[nodiscard]] bool isRawMouseActive() const noexcept;

signals:
    void inputRecorded(RecordedInput input);
    void recordingStopRequested();
    void capturedKey(quint64 encodedKey);
    void captureModifierChanged(KeyDescriptor key, bool pressed);
    void controlHotkeyPressed();
    void physicalKeyChanged(KeyDescriptor key, bool pressed);
    void macroTriggerChanged(KeyDescriptor key, bool pressed);

protected:
    bool event(QEvent* event) override;

private:
    void runHookLoop();

    static LRESULT CALLBACK KeyboardHookProc(int code, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK MouseHookProc(int code, WPARAM wParam, LPARAM lParam);

    std::atomic<bool> recordingActive_{false};
    bool recordingStopKeyDown_ = false;
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
    std::uint8_t physicalKeyboardModifiers_ = 0;
    std::function<bool(const KeyDescriptor&, bool)> selfInjectedFilter_;
    std::function<bool(const KeyDescriptor&)> physicalInputFilter_;
    bool captureActive_ = false;
    ULONGLONG captureStartedTick_ = 0;

    std::atomic<bool> dialogOpen_{false};
    std::atomic<bool> repeatRunning_{false};
    std::atomic<bool> rawKeyboardActive_{false};
    std::atomic<bool> rawMouseActive_{false};
    std::atomic<bool> controlHotkeyPressed_{false};
    std::atomic<ULONGLONG> startupIgnoreUntilTick_{0};
};

Q_DECLARE_METATYPE(RecordedInput)
