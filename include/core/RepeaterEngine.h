#pragma once

#include "AppTypes.h"
#include "InputDriverRouter.h"
#include "PrecisionWaiter.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

struct KeyDescriptorHash {
    std::size_t operator()(const KeyDescriptor& k) const noexcept {
        return (static_cast<std::uint64_t>(k.device) << 32)
            ^ (static_cast<std::uint64_t>(k.modifiers) << 40)
            ^ k.code;
    }
};

struct TaskConfig {
    std::uint64_t id = 0;
    KeyDescriptor key;
    RepeatMode mode = RepeatMode::Toggle;
    int intervalMs = kDefaultDelayMs;
    bool enabled = true;
    bool suppressMixedMode = false;
    ClickPosition clickPos;  // 仅鼠标键使用
    bool martialAssist = false;
    bool preventAccidentalStop = false;
};

struct MartialAssistRuntime {
    enum class Phase { Idle, Holding, GameRepeat, Closing, CloseFailed };

    Phase phase = Phase::Idle;
    HWND window = nullptr;
    std::chrono::steady_clock::time_point nextAction{};
    bool closeForwarded = false;
};

struct TaskRuntime {
    TaskConfig config;
    std::chrono::steady_clock::time_point lastUpTime{};
    bool isDown = false;
    std::chrono::steady_clock::time_point downTime{};
    MartialAssistRuntime martialAssist;
    bool longPressAttemptLogged = false;
    bool longPressForwardedLogged = false;
};

struct RepeaterStatus {
    bool running = false;
    bool paused = false;
    bool holdPriorityActive = false;
    std::optional<KeyDescriptor> independentKey;
    std::optional<std::uint64_t> sequenceTaskId;
    std::vector<KeyDescriptor> holdKeys;
};

class RepeaterEngine {
public:
    explicit RepeaterEngine(InputDriverRouter& driver);
    ~RepeaterEngine();

    void StartThread();
    void StopThread();

    void UpdateBindings(std::uint64_t profileId, const std::vector<KeyBinding>& bindings, bool rememberSequencePosition);
    void ForgetProfileRuntimeState(std::uint64_t profileId);
    void SetToolRunning(bool running);
    bool IsToolRunning() const noexcept;
    void SetMixedMode(bool enabled);
    void SetWasdProtection(bool enabled);
    void SetSequenceSwitchKey(std::optional<KeyDescriptor> key);
    void SetSelfInjectedKeyboardVisible(bool visible);
    void SetSelfInjectedMouseVisible(bool visible);
    void SetPauseKeys(const std::vector<KeyDescriptor>& keys);

    // 前台门控挂起：暂停注入并释放已按下的键，但保留开关状态；恢复后继续。
    void SetSuspended(bool suspended);
    void InvalidateLongPressForeground() noexcept;
    void RefreshLongPressForeground(HWND window, bool allowed);

    void OnPhysicalDown(const KeyDescriptor& key);
    void OnPhysicalUp(const KeyDescriptor& key);
    void ReleaseAll();

    bool HasActiveHoldKeys() const;
    [[nodiscard]] RepeaterStatus GetStatus() const;

private:
    void WorkerLoop();
    void ResetAllLocked();
    void NormalizeIndependentSelectionLocked();
    void ResetIndependentTaskTimingLocked(const KeyDescriptor& key);
    void NormalizeSequenceSelectionLocked();
    void AdvanceSequenceSelectionLocked();
    void RememberSequenceSelectionLocked();
    void ClearRememberedSequenceSelectionLocked();
    [[nodiscard]] bool IsTaskActiveLocked(const TaskRuntime& task,
        std::chrono::steady_clock::time_point now) const;
    [[nodiscard]] bool IsMartialAssistEnabledLocked(const TaskRuntime& task) const;
    [[nodiscard]] bool IsMartialAssistClosingLocked() const;
    [[nodiscard]] bool IsMartialAssistForegroundLocked(HWND window,
        std::chrono::steady_clock::time_point now) const;
    [[nodiscard]] std::optional<std::chrono::steady_clock::time_point> MartialAssistDownSinceLocked(
        const TaskRuntime& task, bool closing) const;
    void AdvanceMartialAssistLocked(std::chrono::steady_clock::time_point now);
    bool SyncOutputsLocked();
    void RefreshPhysicalHeldOutputLocked(const KeyDescriptor& key);
    static std::chrono::steady_clock::time_point Now();

    InputDriverRouter& driver_;

    std::atomic<bool> exit_{false};
    std::atomic<bool> running_{false};
    std::atomic<bool> suspended_{false};
    std::thread worker_;
    PrecisionWaiter waiter_;

    mutable std::mutex mutex_;
    std::vector<TaskRuntime> tasks_;

    std::unordered_set<KeyDescriptor, KeyDescriptorHash> physicalDown_;
    bool toolRunning_ = false;
    bool mixedMode_ = false;
    bool wasdProtection_ = false;
    std::atomic<bool> longPressForegroundInvalidated_{false};
    bool longPressForegroundAllowed_ = true;
    HWND longPressForegroundWindow_ = nullptr;
    std::chrono::steady_clock::time_point longPressResumeTime_{};
    bool selfInjectedKeyboardVisible_ = true;
    bool selfInjectedMouseVisible_ = true;

    std::vector<KeyDescriptor> pauseKeys_;
    std::unordered_set<KeyDescriptor, KeyDescriptorHash> pauseKeysDown_;
    bool IsPauseActive() const;
    bool IsPausedByInputLocked() const;

    std::unordered_map<std::uint64_t, int> ignoreCount_;
    void BeforeSendLocked(const KeyDescriptor& key);
    bool ShouldIgnore(const KeyDescriptor& key);

    std::unordered_set<KeyDescriptor, KeyDescriptorHash> holdPhysicalDown_;
    std::unordered_map<std::uint64_t, int> holdUpIgnoreCount_;
    bool IsHoldBinding(const KeyDescriptor& key) const;
    const TaskConfig* FindIndependentBinding(const KeyDescriptor& key) const;
    bool IsHoldActive() const;
    bool IsWasdProtectionActive() const;
    bool HasSharedLongPressModifierLocked() const;

    std::unordered_set<KeyDescriptor, KeyDescriptorHash> independentDown_;
    std::unordered_map<std::uint64_t, int> independentDownIgnoreCount_;
    std::unordered_map<std::uint64_t, int> independentUpIgnoreCount_;
    std::optional<KeyDescriptor> independentActiveKey_;

    std::optional<KeyDescriptor> sequenceSwitchKey_;
    bool sequenceSwitchDown_ = false;
    std::optional<std::uint64_t> activeProfileId_;
    std::optional<std::uint64_t> sequenceActiveTaskId_;
    bool rememberSequencePosition_ = true;
    std::unordered_map<std::uint64_t, std::uint64_t> sequenceSelectionByProfile_;
    std::unordered_set<std::uint64_t> sourceActiveKeys_;
    struct OutputState {
        KeyDescriptor key;
        bool held = false;
        bool martialAssistClose = false;
        bool longPressPrimary = false;
        bool martialAssist = false;
        bool traceRelease = false;
    };
    std::vector<OutputState> outputs_;
};
