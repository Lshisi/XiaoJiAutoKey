#include "MacroEngine.h"

#include <algorithm>
#include <chrono>

namespace {
std::uint64_t EncodeKey(const KeyDescriptor& key) {
    return (static_cast<std::uint64_t>(key.device == InputDevice::Mouse ? 1 : 0) << 32) | key.code;
}
}  // namespace

MacroEngine::MacroEngine(InputDriver& driver)
    : driver_(driver) {}

MacroEngine::~MacroEngine() {
    StopThread();
}

void MacroEngine::StartThread() {
    if (worker_.joinable()) return;
    exit_ = false;
    worker_ = std::thread(&MacroEngine::WorkerLoop, this);
}

void MacroEngine::StopThread() {
    exit_ = true;
    waiter_.Wake();
    if (worker_.joinable()) worker_.join();
    StopAll();
}

void MacroEngine::UpdateMacros(const std::vector<MacroConfig>& macros) {
    waiter_.Wake();
    std::lock_guard lock(mutex_);

    std::vector<MacroRuntime> updated;
    updated.reserve(macros.size());

    for (const auto& cfg : macros) {
        MacroRuntime rt;
        rt.config = cfg;

        // 保留已有宏的运行状态
        auto it = std::find_if(macros_.begin(), macros_.end(),
            [&](const MacroRuntime& existing) { return existing.config.id == cfg.id; });
        if (it != macros_.end()) {
            rt.active = it->active;
            rt.currentStep = it->currentStep;
            rt.loopsCompleted = it->loopsCompleted;
            rt.lastUpTime = it->lastUpTime;
            rt.waitingDelay = it->waitingDelay;
            rt.keyDown = it->keyDown;
            rt.keyDownTime = it->keyDownTime;
        }

        updated.push_back(std::move(rt));
    }

    macros_ = std::move(updated);
    pauseKeysDown_.clear();
    SyncSourceActiveKeysLocked();
}

void MacroEngine::OnTriggerDown(const KeyDescriptor& key) {
    waiter_.Wake();
    std::vector<KeyDescriptor> releases;
    {
        std::lock_guard lock(mutex_);
        const auto encoded = EncodeKey(key);

    for (auto& rt : macros_) {
        if (std::find(rt.config.pauseKeys.begin(), rt.config.pauseKeys.end(), key) != rt.config.pauseKeys.end()) {
            pauseKeysDown_.insert(encoded);
        }
        if (!rt.config.enabled) continue;
        if (!rt.config.triggerKey.has_value()) continue;
        if (*rt.config.triggerKey != key) continue;

        if (rt.config.triggerMode == MacroTriggerMode::Toggle) {
            // 开关模式：按一次切换
            if (rt.active) {
                QueueCurrentRelease(rt, releases);
                ResetRuntime(rt);
            } else {
                rt.active = true;
                rt.currentStep = 0;
                rt.loopsCompleted = 0;
                rt.lastUpTime = {};
                rt.waitingDelay = false;
                rt.keyDown = false;
                rt.keyDownTime = {};
            }
        } else {
            // 按压模式：按下开始
            if (!rt.active) {
                rt.active = true;
                rt.currentStep = 0;
                rt.loopsCompleted = 0;
                rt.lastUpTime = {};
                rt.waitingDelay = false;
                rt.keyDown = false;
                rt.keyDownTime = {};
            }
        }

        triggerHeld_[encoded] = true;
    }
        SyncSourceActiveKeysLocked();
    }

    for (const auto& release : releases) {
        driver_.SendUp(release, InputSource::Macro);
    }
}

void MacroEngine::OnTriggerUp(const KeyDescriptor& key) {
    waiter_.Wake();
    std::vector<KeyDescriptor> releases;
    {
        std::lock_guard lock(mutex_);
        const auto encoded = EncodeKey(key);
        triggerHeld_[encoded] = false;
        pauseKeysDown_.erase(encoded);

    for (auto& rt : macros_) {
        if (!rt.config.enabled) continue;
        if (!rt.config.triggerKey.has_value()) continue;
        if (*rt.config.triggerKey != key) continue;

        if (rt.config.triggerMode == MacroTriggerMode::Hold) {
            // 按压模式：松开停止
            QueueCurrentRelease(rt, releases);
            ResetRuntime(rt);
        }
    }
        SyncSourceActiveKeysLocked();
    }

    for (const auto& release : releases) {
        driver_.SendUp(release, InputSource::Macro);
    }
}

void MacroEngine::SetSuspended(bool suspended) {
    waiter_.Wake();
    suspended_.store(suspended);
}

void MacroEngine::StopAll() {
    waiter_.Wake();
    {
        std::lock_guard lock(mutex_);
        for (auto& rt : macros_) {
            ResetRuntime(rt);
        }
        triggerHeld_.clear();
        pauseKeysDown_.clear();
        SyncSourceActiveKeysLocked();
    }
}

bool MacroEngine::HasActiveMacro() const {
    std::lock_guard lock(mutex_);
    return std::any_of(macros_.begin(), macros_.end(),
        [](const MacroRuntime& rt) { return rt.active; });
}

bool MacroEngine::IsRunning(std::uint64_t macroId) const {
    std::lock_guard lock(mutex_);
    auto it = std::find_if(macros_.begin(), macros_.end(),
        [macroId](const MacroRuntime& rt) { return rt.config.id == macroId; });
    return it != macros_.end() && it->active;
}

bool MacroEngine::IsPauseActive() const {
    return !pauseKeysDown_.empty();
}

void MacroEngine::SyncSourceActiveKeysLocked() {
    std::vector<KeyDescriptor> activeKeys;
    std::unordered_set<std::uint64_t> activeEncoded;

    for (const auto& runtime : macros_) {
        if (!runtime.active || !runtime.config.enabled) continue;
        for (const auto& step : runtime.config.steps) {
            if (!step.key.IsValid()) continue;
            const auto encoded = EncodeKey(step.key);
            if (activeEncoded.insert(encoded).second) {
                activeKeys.push_back(step.key);
            }
        }
    }

    if (activeEncoded == sourceActiveKeys_) return;
    sourceActiveKeys_ = std::move(activeEncoded);
    driver_.SetSourceActiveKeys(InputSource::Macro, activeKeys);
}

void MacroEngine::ResetRuntime(MacroRuntime& rt) {
    rt.active = false;
    rt.currentStep = 0;
    rt.loopsCompleted = 0;
    rt.lastUpTime = {};
    rt.waitingDelay = false;
    rt.keyDown = false;
    rt.keyDownTime = {};
}

void MacroEngine::QueueCurrentRelease(const MacroRuntime& rt, std::vector<KeyDescriptor>& releases) {
    if (!rt.keyDown || rt.currentStep < 0 || rt.currentStep >= static_cast<int>(rt.config.steps.size())) {
        return;
    }

    const auto& step = rt.config.steps[rt.currentStep];
    if (step.key.IsValid()) {
        releases.push_back(step.key);
    }
}

void MacroEngine::WorkerLoop() {
    while (!exit_.load()) {
        std::vector<std::pair<KeyDescriptor, bool>> toSend;  // key, isDown
        std::optional<std::chrono::steady_clock::time_point> nextWake;

        {
            std::lock_guard lock(mutex_);
            const bool pauseActive = IsPauseActive() || suspended_.load();

            for (auto& rt : macros_) {
                if (!rt.active || !rt.config.enabled) continue;
                if (rt.config.steps.empty()) {
                    rt.active = false;
                    continue;
                }

                if (pauseActive) {
                    if (rt.keyDown && rt.currentStep >= 0 && rt.currentStep < static_cast<int>(rt.config.steps.size())) {
                        const auto now = Now();
                        const auto& step = rt.config.steps[rt.currentStep];
                        if (step.key.IsValid()) {
                            toSend.push_back({step.key, false});
                        }
                        rt.keyDown = false;
                        rt.lastUpTime = now;
                        rt.waitingDelay = true;
                    }
                    continue;
                }

                const auto now = Now();

                // 如果当前步骤已按下，等待统一的脉冲宽度后抬起
                if (rt.keyDown) {
                    if (now - rt.keyDownTime >= std::chrono::milliseconds(kInputPulseWidthMs)) {
                        const auto& step = rt.config.steps[rt.currentStep];
                        if (step.key.IsValid()) {
                            toSend.push_back({step.key, false});  // up
                        }
                        rt.keyDown = false;
                        rt.lastUpTime = now;
                        rt.waitingDelay = true;
                    }
                    continue;
                }

                // 等待步骤间隔
                if (rt.waitingDelay) {
                    const auto& step = rt.config.steps[rt.currentStep];
                    const int delay = std::max(kMinDelayMs, step.delayAfterMs);
                    if (now - rt.lastUpTime < std::chrono::milliseconds(delay)) {
                        continue;
                    }
                    // 间隔结束，前进到下一步
                    rt.waitingDelay = false;
                    rt.currentStep++;

                    // 检查是否完成一轮
                    if (rt.currentStep >= static_cast<int>(rt.config.steps.size())) {
                        rt.currentStep = 0;
                        rt.loopsCompleted++;

                        // 检查循环次数限制（仅开关模式生效，按压模式持续执行直到松开）
                        if (rt.config.triggerMode == MacroTriggerMode::Toggle
                            && rt.config.loopCount > 0
                            && rt.loopsCompleted >= rt.config.loopCount) {
                            ResetRuntime(rt);
                            continue;
                        }
                    }
                }

                // 执行当前步骤：发送 down
                if (!rt.waitingDelay && !rt.keyDown && rt.currentStep < static_cast<int>(rt.config.steps.size())) {
                    const auto& step = rt.config.steps[rt.currentStep];
                    if (step.key.IsValid()) {
                        toSend.push_back({step.key, true});  // down
                    }
                    rt.keyDown = true;
                    rt.keyDownTime = now;
                }
            }
            SyncSourceActiveKeysLocked();

            for (const auto& runtime : macros_) {
                if (!runtime.active || !runtime.config.enabled || runtime.config.steps.empty()) continue;

                std::chrono::steady_clock::time_point deadline{};
                if (runtime.keyDown) {
                    deadline = runtime.keyDownTime + std::chrono::milliseconds(kInputPulseWidthMs);
                } else if (runtime.waitingDelay && runtime.currentStep >= 0
                    && runtime.currentStep < static_cast<int>(runtime.config.steps.size())) {
                    const int delay = std::max(kMinDelayMs, runtime.config.steps[runtime.currentStep].delayAfterMs);
                    deadline = runtime.lastUpTime + std::chrono::milliseconds(delay);
                }
                if (deadline != std::chrono::steady_clock::time_point{}) {
                    nextWake = !nextWake || deadline < *nextWake ? deadline : nextWake;
                }
            }
        }

        // 在锁外发送按键
        for (const auto& [key, down] : toSend) {
            if (down) {
                driver_.SendDown(key, InputSource::Macro);
            } else {
                driver_.SendUp(key, InputSource::Macro);
            }
        }

        waiter_.WaitUntil(nextWake);
    }
}

MacroStatus MacroEngine::GetStatus(std::uint64_t macroId) const {
    std::lock_guard lock(mutex_);
    const auto it = std::find_if(macros_.begin(), macros_.end(),
        [macroId](const MacroRuntime& rt) { return rt.config.id == macroId; });
    const bool running = it != macros_.end() && it->config.enabled && it->active;
    return {running, running && IsPauseActive()};
}

std::chrono::steady_clock::time_point MacroEngine::Now() {
    return std::chrono::steady_clock::now();
}
