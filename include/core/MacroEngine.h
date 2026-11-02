#pragma once

#include "AppTypes.h"
#include "InputDriver.h"
#include "PrecisionWaiter.h"

#include <atomic>
#include <chrono>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

struct MacroRuntime {
    MacroConfig config;
    bool active = false;
    int currentStep = 0;
    int loopsCompleted = 0;
    std::chrono::steady_clock::time_point lastUpTime{};
    bool waitingDelay = false;
    bool keyDown = false;
    std::chrono::steady_clock::time_point keyDownTime{};
};

struct MacroStatus {
    bool running = false;
    bool paused = false;
    int currentStep = -1;
};

class MacroEngine {
public:
    explicit MacroEngine(InputDriver& driver);
    ~MacroEngine();

    void StartThread();
    void StopThread();

    void UpdateMacros(const std::vector<MacroConfig>& macros);

    void OnTriggerDown(const KeyDescriptor& key);
    void OnTriggerUp(const KeyDescriptor& key);

    void StopAll();

    // 前台门控挂起：暂停注入并抬起当前按下的键，保留循环进度。
    void SetSuspended(bool suspended);

    [[nodiscard]] bool HasActiveMacro() const;
    [[nodiscard]] bool IsRunning(std::uint64_t macroId) const;
    [[nodiscard]] MacroStatus GetStatus(std::uint64_t macroId) const;

private:
    void WorkerLoop();
    void ExecuteStep(MacroRuntime& rt);
    static void ResetRuntime(MacroRuntime& rt);
    static void QueueCurrentRelease(const MacroRuntime& rt, std::vector<KeyDescriptor>& releases);
    static std::chrono::steady_clock::time_point Now();
    bool IsPauseActive() const;
    void SyncSourceActiveKeysLocked();

    InputDriver& driver_;

    std::atomic<bool> exit_{false};
    std::atomic<bool> suspended_{false};
    std::thread worker_;
    PrecisionWaiter waiter_;

    mutable std::mutex mutex_;
    std::vector<MacroRuntime> macros_;
    std::unordered_set<std::uint64_t> sourceActiveKeys_;
    std::unordered_map<std::uint64_t, bool> triggerHeld_;
    std::unordered_set<std::uint64_t> pauseKeysDown_;
};
