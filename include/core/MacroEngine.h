#pragma once
#include "AppTypes.h"
#include "InputDriver.h"
#include "PrecisionWaiter.h"
#include <atomic>
#include <chrono>
#include <mutex>
#include <thread>
#include <vector>

enum class MacroPhase { Ready, Holding, Delay };
struct MacroRuntime {
    MacroConfig config;
    bool active = false;
    bool paused = false;
    int currentStep = 0;
    int loopsCompleted = 0;
    MacroPhase phase = MacroPhase::Ready;
    std::chrono::steady_clock::time_point deadline{};
    std::chrono::steady_clock::duration remaining{};
    std::vector<KeyDescriptor> heldKeys;
    std::optional<KeyDescriptor> pulseKey;
    std::wstring error;
};
struct MacroStatus {
    bool running = false;
    bool paused = false;
    int currentStep = -1;
    std::wstring error;
    bool pauseKeyActive = false;
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
    void SetSuspended(bool suspended);
    [[nodiscard]] bool HasActiveMacro() const;
    [[nodiscard]] bool IsRunning(std::uint64_t macroId) const;
    [[nodiscard]] MacroStatus GetStatus(std::uint64_t macroId) const;
private:
    void WorkerLoop();
    void ResetRuntime(MacroRuntime& runtime);
    void RefreshPauseLocked();
    bool SyncOutputsLocked();
    void SyncSourceActiveKeysLocked();
    void AdvanceStepLocked(MacroRuntime& runtime);
    void ExecuteStepLocked(MacroRuntime& runtime);
    static std::chrono::steady_clock::time_point Now();
    InputDriver& driver_;
    std::atomic<bool> exit_{false};
    bool suspended_ = false;
    std::thread worker_;
    PrecisionWaiter waiter_;
    mutable std::mutex mutex_;
    std::vector<MacroRuntime> macros_;
    std::vector<KeyDescriptor> outputs_;
    std::vector<KeyDescriptor> sourceActiveKeys_;
    std::vector<KeyDescriptor> pauseKeysDown_;
    std::vector<KeyDescriptor> triggerKeysDown_;
};
