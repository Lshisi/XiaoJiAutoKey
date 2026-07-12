#pragma once

#include "AppTypes.h"
#include "InputDriver.h"

#include <Windows.h>

#include <atomic>
#include <cstdint>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

struct KeyDescriptorHash {
    std::size_t operator()(const KeyDescriptor& k) const noexcept {
        return (static_cast<std::uint64_t>(k.device) << 32) ^ k.code;
    }
};

struct TaskConfig {
    std::uint64_t id = 0;
    KeyDescriptor key;
    RepeatMode mode = RepeatMode::Toggle;
    int intervalMs = 100;
    bool enabled = true;
    bool suppressMixedMode = false;
};

struct TaskRuntime {
    TaskConfig config;
    ULONGLONG lastFire = 0;
    bool isDown = false;
    ULONGLONG downTime = 0;
};

class RepeaterEngine {
public:
    explicit RepeaterEngine(InputDriver& driver);
    ~RepeaterEngine();

    void StartThread();
    void StopThread();

    void UpdateBindings(const std::vector<KeyBinding>& bindings);
    void SetToolRunning(bool running);
    bool IsToolRunning() const noexcept;
    void SetMixedMode(bool enabled);
    void SetWasdProtection(bool enabled);
    void SetSelfInjectedKeyboardVisible(bool visible);
    void SetSelfInjectedMouseVisible(bool visible);
    void SetPauseKeys(const std::vector<KeyDescriptor>& keys);

    void OnPhysicalDown(const KeyDescriptor& key);
    void OnPhysicalUp(const KeyDescriptor& key);
    void ReleaseAll();

    bool HasActiveHoldKeys() const;

private:
    void WorkerLoop();
    void ResetAllLocked();
    void NormalizeIndependentSelectionLocked();
    void ResetIndependentTaskTimingLocked(const KeyDescriptor& key);
    static ULONGLONG NowMs();

    InputDriver& driver_;

    std::atomic<bool> exit_{false};
    std::atomic<bool> running_{false};
    std::atomic<std::uint64_t> runGeneration_{0};
    std::thread worker_;

    mutable std::mutex mutex_;
    std::vector<TaskRuntime> tasks_;

    std::unordered_set<KeyDescriptor, KeyDescriptorHash> physicalDown_;
    bool toolRunning_ = false;
    bool mixedMode_ = false;
    bool wasdProtection_ = false;
    bool selfInjectedKeyboardVisible_ = true;
    bool selfInjectedMouseVisible_ = true;

    std::vector<KeyDescriptor> pauseKeys_;
    std::unordered_set<KeyDescriptor, KeyDescriptorHash> pauseKeysDown_;
    bool IsPauseActive() const;

    std::unordered_map<std::uint64_t, int> ignoreCount_;
    void BeforeSendLocked(const KeyDescriptor& key);
    bool ShouldIgnore(const KeyDescriptor& key);

    std::unordered_set<KeyDescriptor, KeyDescriptorHash> holdPhysicalDown_;
    std::unordered_map<std::uint64_t, int> holdUpIgnoreCount_;
    bool IsHoldBinding(const KeyDescriptor& key) const;
    bool IsIndependentBinding(const KeyDescriptor& key) const;
    bool IsHoldActive() const;
    bool IsWasdProtectionActive() const;

    std::unordered_set<KeyDescriptor, KeyDescriptorHash> independentDown_;
    std::unordered_map<std::uint64_t, int> independentDownIgnoreCount_;
    std::unordered_map<std::uint64_t, int> independentUpIgnoreCount_;
    std::optional<KeyDescriptor> independentActiveKey_;
};
