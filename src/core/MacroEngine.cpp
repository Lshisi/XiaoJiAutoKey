#include "MacroEngine.h"
#include "MacroActions.h"
#include "ClickPositioning.h"
#include <algorithm>

MacroEngine::MacroEngine(InputDriver& driver) : driver_(driver) {}
MacroEngine::~MacroEngine() { StopThread(); }
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
void MacroEngine::ResetRuntime(MacroRuntime& rt) {
    rt.active = false;
    rt.paused = false;
    rt.currentStep = 0;
    rt.loopsCompleted = 0;
    rt.phase = MacroPhase::Ready;
    rt.deadline = {};
    rt.remaining = {};
    rt.heldKeys.clear();
    rt.pulseKey.reset();
}
void MacroEngine::UpdateMacros(const std::vector<MacroConfig>& macros) {
    std::lock_guard lock(mutex_);
    for (auto& rt : macros_) ResetRuntime(rt);
    SyncOutputsLocked();
    macros_.clear();
    for (const auto& config : macros) {
        MacroRuntime runtime;
        runtime.config = config;
        macros_.push_back(std::move(runtime));
    }
    pauseKeysDown_.clear();
    triggerKeysDown_.clear();
    SyncSourceActiveKeysLocked();
    waiter_.Wake();
}
void MacroEngine::OnTriggerDown(const KeyDescriptor& key) {
    std::lock_guard lock(mutex_);
    if (std::any_of(triggerKeysDown_.begin(), triggerKeysDown_.end(),
        [&key](const auto& down) { return SamePrimaryKey(down, key); })) return;
    triggerKeysDown_.push_back(key);
    for (auto& rt : macros_) {
        if (!rt.config.enabled) continue;
        if (std::any_of(rt.config.pauseKeys.begin(), rt.config.pauseKeys.end(),
            [&key](const auto& pause) { return KeyMatchesEvent(pause, key); })
            && std::find(pauseKeysDown_.begin(), pauseKeysDown_.end(), key) == pauseKeysDown_.end()) {
            pauseKeysDown_.push_back(key);
        }
        if (!rt.config.triggerKey || !KeyMatchesEvent(*rt.config.triggerKey, key)) continue;
        if (rt.active) {
            if (rt.config.triggerMode == MacroTriggerMode::Toggle) ResetRuntime(rt);
            continue;
        }
        rt.error = ValidateMacroSteps(rt.config.steps);
        if (!rt.error.empty()) continue;
        for (const auto& step : rt.config.steps) {
            if (!step.enabled) continue;
            if (step.action == MacroAction::Wheel && !driver_.SupportsMouseWheel()) {
                rt.error = L"当前驱动不支持滚轮，请切换支持滚轮的驱动后启动。";
                break;
            }
            if (MacroActionCanPosition(step.action) && CanUseClickPosition(step.key)
                && step.clickPos.IsFixed() && !driver_.SupportsMouseMove()) {
                rt.error = L"当前驱动不支持固定点位，请修改步骤或切换驱动。";
                break;
            }
        }
        if (!rt.error.empty()) continue;
        const auto first = std::find_if(rt.config.steps.begin(), rt.config.steps.end(),
            [](const auto& step) { return step.enabled; });
        if (first == rt.config.steps.end()) continue;
        rt.active = true;
        rt.currentStep = static_cast<int>(first - rt.config.steps.begin());
    }
    RefreshPauseLocked();
    SyncOutputsLocked();
    SyncSourceActiveKeysLocked();
    waiter_.Wake();
}
void MacroEngine::OnTriggerUp(const KeyDescriptor& key) {
    std::lock_guard lock(mutex_);
    std::erase_if(triggerKeysDown_, [&key](const auto& down) { return SamePrimaryKey(down, key); });
    const auto released = [&key](const KeyDescriptor& binding) {
        return SamePrimaryKey(binding, key) || (IsKeyboardModifierKey(key)
            && (binding.modifiers & ModifierMaskFromKey(key)) != 0);
    };
    std::erase_if(pauseKeysDown_, released);
    for (auto& rt : macros_) {
        if (rt.active && rt.config.triggerMode == MacroTriggerMode::Hold
            && rt.config.triggerKey && released(*rt.config.triggerKey)) ResetRuntime(rt);
    }
    RefreshPauseLocked();
    SyncOutputsLocked();
    SyncSourceActiveKeysLocked();
    waiter_.Wake();
}
void MacroEngine::SetSuspended(bool suspended) {
    std::lock_guard lock(mutex_);
    suspended_ = suspended;
    RefreshPauseLocked();
    SyncOutputsLocked();
    waiter_.Wake();
}
void MacroEngine::RefreshPauseLocked() {
    const auto now = Now();
    const bool paused = suspended_ || !pauseKeysDown_.empty();
    for (auto& rt : macros_) {
        if (!rt.active || rt.paused == paused) continue;
        if (paused && rt.phase == MacroPhase::Holding
            && rt.config.steps[rt.currentStep].action == MacroAction::Click) {
            // 已发送的点击结束后不重发；恢复时只继续点击后的间隔。
            rt.pulseKey.reset();
            rt.phase = MacroPhase::Delay;
            rt.remaining = std::chrono::milliseconds(rt.config.steps[rt.currentStep].delayAfterMs);
        } else if (paused) rt.remaining = std::max(rt.deadline - now, std::chrono::steady_clock::duration::zero());
        else if (rt.phase != MacroPhase::Ready) rt.deadline = now + rt.remaining;
        rt.paused = paused;
    }
}
void MacroEngine::StopAll() {
    std::lock_guard lock(mutex_);
    for (auto& rt : macros_) ResetRuntime(rt);
    pauseKeysDown_.clear();
    triggerKeysDown_.clear();
    SyncOutputsLocked();
    SyncSourceActiveKeysLocked();
    waiter_.Wake();
}
bool MacroEngine::HasActiveMacro() const {
    std::lock_guard lock(mutex_);
    return std::any_of(macros_.begin(), macros_.end(), [](const auto& rt) { return rt.active; });
}
bool MacroEngine::IsRunning(std::uint64_t macroId) const { return GetStatus(macroId).running; }
MacroStatus MacroEngine::GetStatus(std::uint64_t macroId) const {
    std::lock_guard lock(mutex_);
    const auto it = std::find_if(macros_.begin(), macros_.end(),
        [macroId](const auto& rt) { return rt.config.id == macroId; });
    if (it == macros_.end()) return {};
    return {it->active, it->active && it->paused, it->active ? it->currentStep : -1, it->error,
        it->active && !pauseKeysDown_.empty()};
}
void MacroEngine::SyncSourceActiveKeysLocked() {
    std::vector<KeyDescriptor> keys;
    for (const auto& rt : macros_) {
        if (!rt.active) continue;
        for (const auto& step : rt.config.steps) {
            if (step.enabled && MacroActionNeedsKey(step.action)) keys.push_back(step.key);
        }
    }
    if (sourceActiveKeys_ == keys) return;
    sourceActiveKeys_ = keys;
    driver_.SetSourceActiveKeys(InputSource::Macro, keys);
}
bool MacroEngine::SyncOutputsLocked() {
    std::vector<KeyDescriptor> desired;
    const auto append = [&desired](const KeyDescriptor& key) {
        for (const auto& component : ExpandKeyComponents(key)) {
            if (std::find(desired.begin(), desired.end(), component) == desired.end()) desired.push_back(component);
        }
    };
    for (const auto& rt : macros_) {
        if (!rt.active || rt.paused) continue;
        for (const auto& key : rt.heldKeys) append(key);
        if (rt.pulseKey) append(*rt.pulseKey);
    }
    std::stable_sort(desired.begin(), desired.end(), [](const auto& left, const auto& right) {
        return IsKeyboardModifierKey(left) > IsKeyboardModifierKey(right);
    });
    bool ok = true;
    for (std::size_t index = outputs_.size(); index > 0; --index) {
        const auto key = outputs_[index - 1];
        if (std::find(desired.begin(), desired.end(), key) != desired.end()) continue;
        if (driver_.SendUp(key, InputSource::Macro)) outputs_.erase(outputs_.begin() + index - 1);
        else ok = false;
    }
    for (const auto& key : desired) {
        if (std::find(outputs_.begin(), outputs_.end(), key) != outputs_.end()) continue;
        if (driver_.SendHeldDown(key, InputSource::Macro)) outputs_.push_back(key);
        else { ok = false; break; }
    }
    return ok;
}
void MacroEngine::AdvanceStepLocked(MacroRuntime& rt) {
    do {
        ++rt.currentStep;
        if (rt.currentStep >= static_cast<int>(rt.config.steps.size())) {
            rt.currentStep = 0;
            ++rt.loopsCompleted;
            if (rt.config.triggerMode == MacroTriggerMode::Toggle && rt.config.loopCount > 0
                && rt.loopsCompleted >= rt.config.loopCount) {
                ResetRuntime(rt);
                return;
            }
        }
    } while (!rt.config.steps[rt.currentStep].enabled);
    rt.phase = MacroPhase::Ready;
}
void MacroEngine::ExecuteStepLocked(MacroRuntime& rt) {
    const auto& step = rt.config.steps[rt.currentStep];
    if (MacroActionCanPosition(step.action) && CanUseClickPosition(step.key) && step.clickPos.IsFixed()) {
        const auto point = ResolveClickAnchorScreenPoint(step.clickPos);
        if (!point || !MoveCursorToScreenPoint(driver_, point->x, point->y)) {
            rt.error = L"固定点位移动失败，循环已停止。";
            ResetRuntime(rt);
            return;
        }
    }
    switch (step.action) {
    case MacroAction::Click:
    case MacroAction::Hold:
        rt.pulseKey = step.key;
        rt.phase = MacroPhase::Holding;
        rt.deadline = Now() + std::chrono::milliseconds(step.action == MacroAction::Click ? kInputPulseWidthMs : step.durationMs);
        break;
    case MacroAction::Down:
        rt.heldKeys.push_back(step.key);
        rt.phase = MacroPhase::Delay;
        rt.deadline = Now() + std::chrono::milliseconds(step.delayAfterMs);
        break;
    case MacroAction::Up:
        std::erase(rt.heldKeys, step.key);
        rt.phase = MacroPhase::Delay;
        rt.deadline = Now() + std::chrono::milliseconds(step.delayAfterMs);
        break;
    case MacroAction::Wait:
        rt.phase = MacroPhase::Delay;
        rt.deadline = Now() + std::chrono::milliseconds(step.durationMs);
        break;
    case MacroAction::Wheel:
        if (!driver_.SendMouseWheel(step.wheelDelta, InputSource::Macro)) {
            rt.error = L"当前驱动无法发送滚轮，循环已停止。";
            ResetRuntime(rt);
            return;
        }
        rt.phase = MacroPhase::Delay;
        rt.deadline = Now() + std::chrono::milliseconds(step.delayAfterMs);
        break;
    }
    if (MacroActionNeedsKey(step.action)) {
        if (!SyncOutputsLocked()) {
            rt.error = L"按键输出失败，循环已停止。";
            ResetRuntime(rt);
            return;
        }
        const int duration = step.action == MacroAction::Click ? kInputPulseWidthMs
            : step.action == MacroAction::Hold ? step.durationMs : step.delayAfterMs;
        rt.deadline = Now() + std::chrono::milliseconds(duration);
    }
}
void MacroEngine::WorkerLoop() {
    while (!exit_) {
        std::optional<std::chrono::steady_clock::time_point> nextWake;
        {
            std::lock_guard lock(mutex_);
            for (auto& rt : macros_) {
                if (!rt.active || rt.paused) continue;
                if (rt.phase == MacroPhase::Holding && Now() >= rt.deadline) {
                    rt.pulseKey.reset();
                    // 间隔为 0 且下一步使用同一键时，也必须先发出本次松开。
                    if (!SyncOutputsLocked()) {
                        rt.error = L"按键释放失败，循环已停止。";
                        ResetRuntime(rt);
                        continue;
                    }
                    rt.phase = MacroPhase::Delay;
                    rt.deadline = Now() + std::chrono::milliseconds(rt.config.steps[rt.currentStep].delayAfterMs);
                }
                if (rt.phase == MacroPhase::Delay && Now() >= rt.deadline) AdvanceStepLocked(rt);
                if (rt.active && rt.phase == MacroPhase::Ready) ExecuteStepLocked(rt);
                if (rt.active && (!nextWake || rt.deadline < *nextWake)) nextWake = rt.deadline;
            }
            if (!SyncOutputsLocked()) {
                for (auto& rt : macros_) {
                    if (rt.active) rt.error = L"按键输出失败，循环已停止。";
                    ResetRuntime(rt);
                }
                SyncOutputsLocked();
                nextWake.reset();
            }
            SyncSourceActiveKeysLocked();
        }
        waiter_.WaitUntil(nextWake);
    }
}
std::chrono::steady_clock::time_point MacroEngine::Now() { return std::chrono::steady_clock::now(); }
