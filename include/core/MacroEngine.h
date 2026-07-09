#pragma once

#include "AppTypes.h"
#include "InputDriver.h"

#include <Windows.h>

#include <atomic>
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
    ULONGLONG lastStepTime = 0;
    bool waitingDelay = false;
    bool keyDown = false;
    ULONGLONG keyDownTime = 0;
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

    [[nodiscard]] bool HasActiveMacro() const;
    [[nodiscard]] bool IsRunning(std::uint64_t macroId) const;

private:
    void WorkerLoop();
    void ExecuteStep(MacroRuntime& rt);
    static void ResetRuntime(MacroRuntime& rt);
    static void QueueCurrentRelease(const MacroRuntime& rt, std::vector<KeyDescriptor>& releases);
    static ULONGLONG NowMs();
    bool IsPauseActive() const;

    InputDriver& driver_;

    std::atomic<bool> exit_{false};
    std::thread worker_;

    mutable std::mutex mutex_;
    std::vector<MacroRuntime> macros_;
    std::unordered_map<std::uint64_t, bool> triggerHeld_;
    std::unordered_set<std::uint64_t> pauseKeysDown_;
};
