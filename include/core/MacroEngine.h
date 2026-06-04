#pragma once

#include "AppTypes.h"
#include "DdDriver.h"

#include <Windows.h>

#include <atomic>
#include <mutex>
#include <thread>
#include <unordered_map>
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
    explicit MacroEngine(DdDriver& driver);
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
    static ULONGLONG NowMs();

    DdDriver& driver_;

    std::atomic<bool> exit_{false};
    std::thread worker_;

    mutable std::mutex mutex_;
    std::vector<MacroRuntime> macros_;
    std::unordered_map<std::uint64_t, bool> triggerHeld_;
};
