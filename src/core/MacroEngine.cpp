#include "MacroEngine.h"

#include "ClickPositioning.h"

#include <algorithm>
#include <chrono>

namespace {
std::uint64_t EncodeKey(const KeyDescriptor& key) {
    return (static_cast<std::uint64_t>(key.device == InputDevice::Mouse ? 1 : 0) << 32)
        | (static_cast<std::uint64_t>(key.modifiers) << 40)
        | key.code;
}

KeyDescriptor DecodeKey(std::uint64_t encoded) {
    KeyDescriptor key;
    key.device = ((encoded >> 32) & 0x1) != 0 ? InputDevice::Mouse : InputDevice::Keyboard;
    key.code = static_cast<std::uint32_t>(encoded & 0xFFFFFFFFull);
    key.modifiers = static_cast<std::uint8_t>((encoded >> 40) & 0xFFull);
    return key;
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
        if (std::any_of(rt.config.pauseKeys.begin(), rt.config.pauseKeys.end(),
            [&key](const KeyDescriptor& pauseKey) { return KeyMatchesEvent(pauseKey, key); })) {
            pauseKeysDown_.insert(encoded);
        }
        if (!rt.config.enabled) continue;
        if (!rt.config.triggerKey.has_value()) continue;
        if (!KeyMatchesEvent(*rt.config.triggerKey, key)) continue;

        const auto firstStep = std::find_if(rt.config.steps.begin(), rt.config.steps.end(),
            [](const MacroStep& step) { return step.enabled; });
        if (firstStep == rt.config.steps.end()) continue;
        const int firstStepIndex = static_cast<int>(firstStep - rt.config.steps.begin());

        if (rt.config.triggerMode == MacroTriggerMode::Toggle) {
            // 开关模式：按一次切换
            if (rt.active) {
                QueueCurrentRelease(rt, releases);
                ResetRuntime(rt);
            } else {
                rt.active = true;
                rt.currentStep = firstStepIndex;
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
                rt.currentStep = firstStepIndex;
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
        const auto stopHoldTrigger = [&](MacroRuntime& runtime) {
            if (runtime.config.triggerMode != MacroTriggerMode::Hold) return;
            QueueCurrentRelease(runtime, releases);
            ResetRuntime(runtime);
        };

        if (IsKeyboardModifierKey(key)) {
            const auto modifierMask = ModifierMaskFromKey(key);
            const auto usesModifier = [modifierMask](const KeyDescriptor& item) {
                return (item.modifiers & modifierMask) != 0;
            };
            std::erase_if(pauseKeysDown_, [&usesModifier](const std::uint64_t encodedKey) {
                return usesModifier(DecodeKey(encodedKey));
            });
            for (auto& [triggerEncoded, held] : triggerHeld_) {
                if (usesModifier(DecodeKey(triggerEncoded))) held = false;
            }
            for (auto& rt : macros_) {
                if (!rt.config.enabled || !rt.config.triggerKey.has_value()
                    || !usesModifier(*rt.config.triggerKey)) {
                    continue;
                }
                stopHoldTrigger(rt);
            }
            SyncSourceActiveKeysLocked();
        } else {
            for (auto& [triggerEncoded, held] : triggerHeld_) {
                if (SamePrimaryKey(DecodeKey(triggerEncoded), key)) held = false;
            }
            std::erase_if(pauseKeysDown_, [&key](const std::uint64_t encodedKey) {
                return SamePrimaryKey(DecodeKey(encodedKey), key);
            });
            for (auto& rt : macros_) {
                if (!rt.config.enabled) continue;
                if (!rt.config.triggerKey.has_value()) continue;
                if (!SamePrimaryKey(*rt.config.triggerKey, key)) continue;

                stopHoldTrigger(rt);
            }
            SyncSourceActiveKeysLocked();
        }

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
            if (!step.enabled || !step.key.IsValid()) continue;
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
    struct PendingSend {
        KeyDescriptor key;
        bool down = false;
        ClickPosition clickPos;  // 仅鼠标步骤 down 时使用
    };
    while (!exit_.load()) {
        std::vector<PendingSend> toSend;
        std::optional<std::chrono::steady_clock::time_point> nextWake;

        {
            std::lock_guard lock(mutex_);
            const bool pauseActive = IsPauseActive() || suspended_.load();

            for (auto& rt : macros_) {
                if (!rt.active || !rt.config.enabled) continue;
                if (std::none_of(rt.config.steps.begin(), rt.config.steps.end(),
                    [](const MacroStep& step) { return step.enabled; })) {
                    ResetRuntime(rt);
                    continue;
                }

                if (pauseActive) {
                    if (rt.keyDown && rt.currentStep >= 0 && rt.currentStep < static_cast<int>(rt.config.steps.size())) {
                        const auto now = Now();
                        const auto& step = rt.config.steps[rt.currentStep];
                        if (step.key.IsValid()) {
                            toSend.push_back({step.key, false, {}});
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
                            toSend.push_back({step.key, false, {}});  // up
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
                    do {
                        ++rt.currentStep;
                        if (rt.currentStep >= static_cast<int>(rt.config.steps.size())) {
                            rt.currentStep = 0;
                            ++rt.loopsCompleted;
                            if (rt.config.triggerMode == MacroTriggerMode::Toggle
                                && rt.config.loopCount > 0
                                && rt.loopsCompleted >= rt.config.loopCount) {
                                ResetRuntime(rt);
                                break;
                            }
                        }
                    } while (!rt.config.steps[rt.currentStep].enabled);
                    if (!rt.active) continue;
                }

                // 执行当前步骤：发送 down
                if (!rt.waitingDelay && !rt.keyDown && rt.currentStep < static_cast<int>(rt.config.steps.size())) {
                    const auto& step = rt.config.steps[rt.currentStep];
                    if (step.key.IsValid()) {
                        toSend.push_back({step.key, true,
                            step.key.device == InputDevice::Mouse ? step.clickPos : ClickPosition{}});  // down
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
        for (const auto& send : toSend) {
            if (send.down) {
                if (send.key.device == InputDevice::Mouse && send.clickPos.IsFixed()) {
                    if (const auto point = ResolveClickAnchorScreenPoint(send.clickPos)) {
                        MoveCursorToScreenPoint(driver_, point->x, point->y);
                    }
                }
                driver_.SendDown(send.key, InputSource::Macro);
            } else {
                driver_.SendUp(send.key, InputSource::Macro);
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
    return {running, running && IsPauseActive(), running ? it->currentStep : -1};
}

std::chrono::steady_clock::time_point MacroEngine::Now() {
    return std::chrono::steady_clock::now();
}
