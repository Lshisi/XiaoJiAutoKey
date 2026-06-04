#include "MacroEngine.h"

#include <algorithm>
#include <chrono>

namespace {
std::uint64_t EncodeKey(const KeyDescriptor& key) {
    return (static_cast<std::uint64_t>(key.device == InputDevice::Mouse ? 1 : 0) << 32) | key.code;
}
}  // namespace

MacroEngine::MacroEngine(DdDriver& driver)
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
    if (worker_.joinable()) worker_.join();
    StopAll();
}

void MacroEngine::UpdateMacros(const std::vector<MacroConfig>& macros) {
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
            rt.lastStepTime = it->lastStepTime;
            rt.waitingDelay = it->waitingDelay;
        }

        updated.push_back(std::move(rt));
    }

    macros_ = std::move(updated);
}

void MacroEngine::OnTriggerDown(const KeyDescriptor& key) {
    std::lock_guard lock(mutex_);
    const auto encoded = EncodeKey(key);

    for (auto& rt : macros_) {
        if (!rt.config.enabled) continue;
        if (!rt.config.triggerKey.has_value()) continue;
        if (*rt.config.triggerKey != key) continue;

        if (rt.config.triggerMode == MacroTriggerMode::Toggle) {
            // 开关模式：按一次切换
            if (rt.active) {
                rt.active = false;
            } else {
                rt.active = true;
                rt.currentStep = 0;
                rt.loopsCompleted = 0;
                rt.lastStepTime = 0;
                rt.waitingDelay = false;
                rt.keyDown = false;
            }
        } else {
            // 按压模式：按下开始
            if (!rt.active) {
                rt.active = true;
                rt.currentStep = 0;
                rt.loopsCompleted = 0;
                rt.lastStepTime = 0;
                rt.waitingDelay = false;
                rt.keyDown = false;
            }
        }

        triggerHeld_[encoded] = true;
    }
}

void MacroEngine::OnTriggerUp(const KeyDescriptor& key) {
    std::lock_guard lock(mutex_);
    const auto encoded = EncodeKey(key);
    triggerHeld_[encoded] = false;

    for (auto& rt : macros_) {
        if (!rt.config.enabled) continue;
        if (!rt.config.triggerKey.has_value()) continue;
        if (*rt.config.triggerKey != key) continue;

        if (rt.config.triggerMode == MacroTriggerMode::Hold) {
            // 按压模式：松开停止
            rt.active = false;
        }
    }
}

void MacroEngine::StopAll() {
    std::lock_guard lock(mutex_);
    for (auto& rt : macros_) {
        rt.active = false;
        rt.currentStep = 0;
        rt.loopsCompleted = 0;
        rt.lastStepTime = 0;
        rt.waitingDelay = false;
        rt.keyDown = false;
        rt.keyDownTime = 0;
    }
    triggerHeld_.clear();
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

void MacroEngine::WorkerLoop() {
    while (!exit_.load()) {
        std::vector<std::pair<KeyDescriptor, bool>> toSend;  // key, isDown

        {
            std::lock_guard lock(mutex_);

            for (auto& rt : macros_) {
                if (!rt.active || !rt.config.enabled) continue;
                if (rt.config.steps.empty()) {
                    rt.active = false;
                    continue;
                }

                const auto now = NowMs();

                // 如果当前步骤已按下，等待10ms后抬起
                if (rt.keyDown) {
                    if (now - rt.keyDownTime >= 10) {
                        const auto& step = rt.config.steps[rt.currentStep];
                        if (step.key.IsValid()) {
                            toSend.push_back({step.key, false});  // up
                        }
                        rt.keyDown = false;
                        rt.lastStepTime = now;
                        rt.waitingDelay = true;
                    }
                    continue;
                }

                // 等待步骤间隔
                if (rt.waitingDelay) {
                    const auto& step = rt.config.steps[rt.currentStep];
                    const int delay = std::max(1, step.delayAfterMs);
                    if (now - rt.lastStepTime < static_cast<ULONGLONG>(delay)) {
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
                            rt.active = false;
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
        }

        // 在锁外发送按键
        for (const auto& [key, down] : toSend) {
            if (down) {
                driver_.SendDown(key, InputSource::Injected);
            } else {
                driver_.SendUp(key, InputSource::Injected);
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

ULONGLONG MacroEngine::NowMs() {
    return ::GetTickCount64();
}
