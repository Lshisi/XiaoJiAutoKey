#include "RepeaterEngine.h"

#include "ClickPositioning.h"
#include "DiagLog.h"

#include <algorithm>

namespace {
constexpr int kWasdProtectionMinIntervalMs = 35;
constexpr int kLongPressFocusSettleMs = 50;
constexpr auto kMartialAssistHold = std::chrono::milliseconds(3100);
constexpr auto kMartialAssistReleaseGap = std::chrono::milliseconds(kInputPulseWidthMs);
constexpr auto kMartialAssistOwnershipPoll = std::chrono::milliseconds(50);
using AssistPhase = MartialAssistRuntime::Phase;

std::uint64_t EncodeKey(const KeyDescriptor& key) {
    return (static_cast<std::uint64_t>(key.device == InputDevice::Mouse ? 1 : 0) << 32)
        | (static_cast<std::uint64_t>(key.modifiers) << 40)
        | key.code;
}

bool IsWasdKey(const KeyDescriptor& key) {
    if (key.device != InputDevice::Keyboard) return false;
    switch (key.code) {
    case 'W':
    case 'A':
    case 'S':
    case 'D':
        return true;
    default:
        return false;
    }
}
}  // namespace

RepeaterEngine::RepeaterEngine(InputDriverRouter& driver)
    : driver_(driver) {}

RepeaterEngine::~RepeaterEngine() {
    StopThread();
}

void RepeaterEngine::StartThread() {
    if (worker_.joinable()) return;
    exit_ = false;
    worker_ = std::thread(&RepeaterEngine::WorkerLoop, this);
}

void RepeaterEngine::StopThread() {
    exit_ = true;
    waiter_.Wake();
    if (worker_.joinable()) worker_.join();
    ReleaseAll();
}

bool RepeaterEngine::IsToolRunning() const noexcept {
    return running_.load();
}

void RepeaterEngine::SetMixedMode(bool enabled) {
    std::lock_guard lock(mutex_);
    waiter_.Wake();
    mixedMode_ = enabled;
    SyncOutputsLocked();
}

void RepeaterEngine::SetWasdProtection(bool enabled) {
    std::lock_guard lock(mutex_);
    waiter_.Wake();
    wasdProtection_ = enabled;
}

void RepeaterEngine::SetSequenceSwitchKey(std::optional<KeyDescriptor> key) {
    std::lock_guard lock(mutex_);
    waiter_.Wake();
    sequenceSwitchKey_ = key;
    sequenceSwitchDown_ = false;
}

void RepeaterEngine::SetSelfInjectedKeyboardVisible(bool visible) {
    std::lock_guard lock(mutex_);
    if (selfInjectedKeyboardVisible_ == visible) {
        return;
    }
    selfInjectedKeyboardVisible_ = visible;
    ignoreCount_.clear();
    holdUpIgnoreCount_.clear();
    independentDownIgnoreCount_.clear();
    independentUpIgnoreCount_.clear();
}

void RepeaterEngine::SetSelfInjectedMouseVisible(bool visible) {
    std::lock_guard lock(mutex_);
    if (selfInjectedMouseVisible_ == visible) {
        return;
    }
    selfInjectedMouseVisible_ = visible;
    ignoreCount_.clear();
    holdUpIgnoreCount_.clear();
    independentDownIgnoreCount_.clear();
    independentUpIgnoreCount_.clear();
}

void RepeaterEngine::SetPauseKeys(const std::vector<KeyDescriptor>& keys) {
    std::lock_guard lock(mutex_);
    waiter_.Wake();
    pauseKeys_ = keys;
    pauseKeysDown_.clear();
    SyncOutputsLocked();
}

void RepeaterEngine::SetSuspended(bool suspended) {
    std::lock_guard lock(mutex_);
    suspended_.store(suspended);
    SyncOutputsLocked();
    waiter_.Wake();
}

void RepeaterEngine::InvalidateLongPressForeground() noexcept {
    longPressForegroundInvalidated_.store(true);
    waiter_.Wake();
}

void RepeaterEngine::RefreshLongPressForeground(HWND window, bool allowed) {
    std::lock_guard lock(mutex_);
    longPressForegroundInvalidated_.store(false);
    longPressForegroundWindow_ = window;
    longPressForegroundAllowed_ = allowed;
    // Let focus acquisition settle before restoring a held key in the new window.
    longPressResumeTime_ = Now() + std::chrono::milliseconds(kLongPressFocusSettleMs);
    const bool sharedModifier = HasSharedLongPressModifierLocked();
    for (auto& task : tasks_) {
        if (task.config.mode != RepeatMode::LongPress && task.config.key.modifiers == 0 && !sharedModifier) continue;
        task.isDown = false;
        task.downTime = {};
        task.lastUpTime = {};
    }
    SyncOutputsLocked();
    waiter_.Wake();
}

bool RepeaterEngine::IsPauseActive() const {
    return !pauseKeysDown_.empty();
}

bool RepeaterEngine::IsPausedByInputLocked() const {
    return IsPauseActive() || (mixedMode_ && IsHoldActive());
}

// ── 启停 ─────────────────────────────
void RepeaterEngine::SetToolRunning(bool running) {
    std::lock_guard lock(mutex_);
    waiter_.Wake();
    toolRunning_ = running;
    running_.store(running);

    physicalDown_.clear();
    ignoreCount_.clear();
    holdPhysicalDown_.clear();
    holdUpIgnoreCount_.clear();
    pauseKeysDown_.clear();
    independentDown_.clear();
    independentDownIgnoreCount_.clear();
    independentUpIgnoreCount_.clear();
    independentActiveKey_.reset();
    sequenceSwitchDown_ = false;
    sequenceActiveTaskId_.reset();

    for (auto& task : tasks_) {
        task.lastUpTime = {};
        task.isDown = false;
        task.downTime = {};
        if (running) {
            task.longPressAttemptLogged = false;
            task.longPressForwardedLogged = false;
        }
    }

    if (running) {
        NormalizeSequenceSelectionLocked();
    }

    if (std::any_of(tasks_.begin(), tasks_.end(), [](const auto& task) {
            return task.config.enabled && task.config.mode == RepeatMode::LongPress;
        })) {
        DiagLog::Write(std::string(running ? "long press start" : "long press stop")
            + " suspended=" + std::to_string(suspended_.load())
            + " foregroundAllowed=" + std::to_string(longPressForegroundAllowed_)
            + " foregroundSettling=" + std::to_string(longPressForegroundInvalidated_.load() || Now() < longPressResumeTime_));
    }

    SyncOutputsLocked();
}

// ── 物理输入状态追踪 ─────────────
void RepeaterEngine::OnPhysicalDown(const KeyDescriptor& key) {
    std::lock_guard lock(mutex_);
    waiter_.Wake();
    const auto encoded = EncodeKey(key);

    if (sequenceSwitchKey_.has_value() && KeyMatchesEvent(*sequenceSwitchKey_, key)) {
        if (!sequenceSwitchDown_) {
            sequenceSwitchDown_ = true;
            AdvanceSequenceSelectionLocked();
            SyncOutputsLocked();
        }
        return;
    }

    // 暂停键追踪
    for (const auto& pk : pauseKeys_) {
        if (KeyMatchesEvent(pk, key)) {
            pauseKeysDown_.insert(key);
            break;
        }
    }

    auto it = ignoreCount_.find(encoded);

    if (const auto* binding = FindIndependentBinding(key)) {
        auto independentIgnoreIt = independentDownIgnoreCount_.find(encoded);
        if (independentIgnoreIt != independentDownIgnoreCount_.end() && independentIgnoreIt->second > 0) {
            --independentIgnoreIt->second;
            return;
        }
        if (independentDown_.count(key) > 0) {
            return;
        }
        independentDown_.insert(key);
        physicalDown_.insert(key);
        if (!independentActiveKey_ || *independentActiveKey_ != key) {
            independentActiveKey_ = key;
            ResetIndependentTaskTimingLocked(key);
        } else if (!binding->preventAccidentalStop) {
            independentActiveKey_.reset();
        }
        SyncOutputsLocked();
        return;
    }

    // ⭐ Hold模式key的独立追踪
    if (IsHoldBinding(key)) {
        // KEYDOWN：只要key不在集合中就加入（首次物理按下）
        // 如果已在集合中，忽略（键盘重复或SendTap的KEYDOWN都无害）
        if (holdPhysicalDown_.count(key) == 0) {
            holdPhysicalDown_.insert(key);
        }
    }

    // 原有的physicalDown_追踪（用于Hold模式的isDown判断）
    if (physicalDown_.count(key) > 0) {
        if (it != ignoreCount_.end() && it->second > 0) {
            --it->second;
        }
        return;
    }

    if (it != ignoreCount_.end() && it->second > 0) {
        --it->second;
        return;
    }
    physicalDown_.insert(key);
    const auto now = Now();
    for (auto& task : tasks_) {
        auto& assist = task.martialAssist;
        if (assist.phase == AssistPhase::ManualHolding) {
            const auto modifier = ModifierMaskFromKey(key);
            if (modifier != 0 && (task.config.key.modifiers & modifier) == 0) {
                assist.manualHoldInterrupted = true;
            }
            continue;
        }
        if (toolRunning_ && IsMartialAssistEnabledLocked(task)
            && (assist.phase == AssistPhase::GameRepeat || assist.phase == AssistPhase::Closing)
            && KeyMatchesEvent(task.config.key, key) && IsMartialAssistForegroundLocked(assist.window, now)) {
            assist = {AssistPhase::ManualHolding, assist.window, {}, now};
            DiagLog::Write("martial assist manual down code=" + std::to_string(key.code));
        }
    }
    SyncOutputsLocked();
}

void RepeaterEngine::OnPhysicalUp(const KeyDescriptor& key) {
    std::lock_guard lock(mutex_);
    waiter_.Wake();
    const auto encoded = EncodeKey(key);
    const auto now = Now();
    for (auto& task : tasks_) {
        auto& assist = task.martialAssist;
        if (assist.phase != AssistPhase::ManualHolding) continue;
        if (SamePrimaryKey(task.config.key, key)) {
            const bool gameRepeat = !assist.manualHoldInterrupted
                && now >= assist.manualDownTime + kMartialAssistHold
                && IsMartialAssistForegroundLocked(assist.window, now);
            assist = {gameRepeat ? AssistPhase::GameRepeat : AssistPhase::Idle,
                assist.window, now + kMartialAssistReleaseGap};
            task.longPressAttemptLogged = false;
            task.longPressForwardedLogged = false;
            DiagLog::Write("martial assist manual up code=" + std::to_string(key.code)
                + " result=" + (gameRepeat ? "game repeat" : "restart pending"));
        } else if ((task.config.key.modifiers & ModifierMaskFromKey(key)) != 0) {
            assist.manualHoldInterrupted = true;
        }
    }

    if (IsKeyboardModifierKey(key)) {
        const auto modifierMask = ModifierMaskFromKey(key);
        const auto usesModifier = [modifierMask](const KeyDescriptor& item) {
            return (item.modifiers & modifierMask) != 0;
        };
        const auto released = [&key, &usesModifier](const KeyDescriptor& item) {
            return SamePrimaryKey(item, key) || usesModifier(item);
        };
        std::erase_if(physicalDown_, released);
        std::erase_if(pauseKeysDown_, released);
        std::erase_if(holdPhysicalDown_, released);
        std::erase_if(independentDown_, released);
        if (sequenceSwitchKey_.has_value() && usesModifier(*sequenceSwitchKey_)) {
            sequenceSwitchDown_ = false;
        }
        RefreshPhysicalHeldOutputLocked(key);
        SyncOutputsLocked();
        return;
    }

    if (sequenceSwitchKey_.has_value() && SamePrimaryKey(*sequenceSwitchKey_, key)) {
        sequenceSwitchDown_ = false;
        return;
    }

    // 暂停键追踪
    std::erase_if(pauseKeysDown_, [&key](const KeyDescriptor& item) {
        return SamePrimaryKey(item, key);
    });

    if (FindIndependentBinding(key)) {
        auto independentIgnoreIt = independentUpIgnoreCount_.find(encoded);
        if (independentIgnoreIt != independentUpIgnoreCount_.end() && independentIgnoreIt->second > 0) {
            --independentIgnoreIt->second;
            return;
        }
        std::erase_if(independentDown_, [&key](const KeyDescriptor& item) {
            return SamePrimaryKey(item, key);
        });
        std::erase_if(physicalDown_, [&key](const KeyDescriptor& item) {
            return SamePrimaryKey(item, key);
        });
        SyncOutputsLocked();
        return;
    }

    // ⭐ Hold模式key的独立追踪：用独立的KEYUP计数器过滤SendTap的KEYUP
    if (IsHoldBinding(key)) {
        auto it = holdUpIgnoreCount_.find(encoded);
        if (it != holdUpIgnoreCount_.end() && it->second > 0) {
            --it->second;
            // SendTap产生的KEYUP，不移除
        } else {
            // 真实物理松开
            std::erase_if(holdPhysicalDown_, [&key](const KeyDescriptor& item) {
                return SamePrimaryKey(item, key);
            });
        }
    }

    // 原有的physicalDown_追踪
    auto it = ignoreCount_.find(encoded);
    if (it != ignoreCount_.end() && it->second > 0) {
        --it->second;
        return;
    }
    std::erase_if(physicalDown_, [&key](const KeyDescriptor& item) {
        return SamePrimaryKey(item, key);
    });
    RefreshPhysicalHeldOutputLocked(key);
    SyncOutputsLocked();
}

void RepeaterEngine::BeforeSendLocked(const KeyDescriptor& key) {
    if ((key.device == InputDevice::Keyboard && !selfInjectedKeyboardVisible_) ||
        (key.device == InputDevice::Mouse && !selfInjectedMouseVisible_)) {
        return;
    }

    const auto encoded = EncodeKey(key);
    if (FindIndependentBinding(key)) {
        independentDownIgnoreCount_[encoded] += 1;
        independentUpIgnoreCount_[encoded] += 1;
        return;
    }

    // down 产生1个事件，up 产生1个事件，总共还是2个需要忽略
    // 但现在 down 和 up 分开调用，down 时先加2（预留给后续的 up 事件）
    ignoreCount_[encoded] += 2;

    // ⭐ 对Hold key，只给KEYUP计数+1（KEYDOWN不需要计数，因为key已在集合中时直接忽略）
    if (IsHoldBinding(key)) {
        holdUpIgnoreCount_[encoded] += 1;
    }
}

bool RepeaterEngine::IsHoldBinding(const KeyDescriptor& key) const {
    return std::any_of(tasks_.begin(), tasks_.end(), [&](const TaskRuntime& t) {
        return t.config.enabled && t.config.mode == RepeatMode::Hold
            && KeyMatchesEvent(t.config.key, key);
    });
}

const TaskConfig* RepeaterEngine::FindIndependentBinding(const KeyDescriptor& key) const {
    const auto match = std::find_if(tasks_.begin(), tasks_.end(), [&](const TaskRuntime& task) {
        return task.config.enabled && task.config.mode == RepeatMode::Independent
            && KeyMatchesEvent(task.config.key, key);
    });
    return match == tasks_.end() ? nullptr : &match->config;
}

bool RepeaterEngine::IsHoldActive() const {
    return std::any_of(tasks_.begin(), tasks_.end(), [&](const TaskRuntime& t) {
        return t.config.enabled
            && t.config.mode == RepeatMode::Hold
            && !t.config.suppressMixedMode
            && holdPhysicalDown_.count(t.config.key) > 0;
    });
}

void RepeaterEngine::NormalizeIndependentSelectionLocked() {
    if (!independentActiveKey_.has_value()) {
        return;
    }

    const KeyDescriptor key = *independentActiveKey_;
    const bool stillValid = std::any_of(tasks_.begin(), tasks_.end(), [&](const TaskRuntime& t) {
        return t.config.enabled && t.config.mode == RepeatMode::Independent && t.config.key == key;
    });
    if (!stillValid) {
        independentActiveKey_.reset();
    }
}

void RepeaterEngine::ResetIndependentTaskTimingLocked(const KeyDescriptor& key) {
    for (auto& t : tasks_) {
        if (t.config.enabled && t.config.mode == RepeatMode::Independent && t.config.key == key) {
            t.lastUpTime = {};
        }
    }
}

void RepeaterEngine::NormalizeSequenceSelectionLocked() {
    if (sequenceActiveTaskId_.has_value()) {
        const bool stillValid = std::any_of(tasks_.begin(), tasks_.end(), [&](const TaskRuntime& t) {
            return t.config.enabled
                && t.config.mode == RepeatMode::Sequence
                && t.config.id == *sequenceActiveTaskId_;
        });
        if (stillValid) {
            RememberSequenceSelectionLocked();
            return;
        }
    }

    sequenceActiveTaskId_.reset();

    if (rememberSequencePosition_ && activeProfileId_.has_value()) {
        const auto remembered = sequenceSelectionByProfile_.find(*activeProfileId_);
        if (remembered != sequenceSelectionByProfile_.end()) {
            const bool stillValid = std::any_of(tasks_.begin(), tasks_.end(), [&](const TaskRuntime& t) {
                return t.config.enabled
                    && t.config.mode == RepeatMode::Sequence
                    && t.config.id == remembered->second;
            });
            if (stillValid) {
                sequenceActiveTaskId_ = remembered->second;
                return;
            }
            sequenceSelectionByProfile_.erase(remembered);
        }
    }

    if (!toolRunning_) {
        return;
    }

    const auto first = std::find_if(tasks_.begin(), tasks_.end(), [](const TaskRuntime& t) {
        return t.config.enabled && t.config.mode == RepeatMode::Sequence;
    });
    if (first != tasks_.end()) {
        sequenceActiveTaskId_ = first->config.id;
        RememberSequenceSelectionLocked();
    }
}

void RepeaterEngine::AdvanceSequenceSelectionLocked() {
    if (!toolRunning_) {
        return;
    }

    std::vector<std::size_t> sequenceIndexes;
    sequenceIndexes.reserve(tasks_.size());
    for (std::size_t i = 0; i < tasks_.size(); ++i) {
        if (tasks_[i].config.enabled && tasks_[i].config.mode == RepeatMode::Sequence) {
            sequenceIndexes.push_back(i);
        }
    }
    if (sequenceIndexes.empty()) {
        sequenceActiveTaskId_.reset();
        ClearRememberedSequenceSelectionLocked();
        return;
    }

    std::size_t nextPosition = 0;
    if (sequenceActiveTaskId_.has_value()) {
        const auto current = std::find_if(sequenceIndexes.begin(), sequenceIndexes.end(), [&](std::size_t index) {
            return tasks_[index].config.id == *sequenceActiveTaskId_;
        });
        if (current != sequenceIndexes.end()) {
            auto& currentTask = tasks_[*current];
            if (currentTask.isDown) {
                currentTask.isDown = false;
                currentTask.lastUpTime = Now();
            }
            nextPosition = (static_cast<std::size_t>(std::distance(sequenceIndexes.begin(), current)) + 1)
                % sequenceIndexes.size();
        }
    }

    auto& nextTask = tasks_[sequenceIndexes[nextPosition]];
    sequenceActiveTaskId_ = nextTask.config.id;
    RememberSequenceSelectionLocked();
    nextTask.lastUpTime = {};
}

void RepeaterEngine::RememberSequenceSelectionLocked() {
    if (!rememberSequencePosition_ || !activeProfileId_.has_value() || !sequenceActiveTaskId_.has_value()) {
        return;
    }
    sequenceSelectionByProfile_[*activeProfileId_] = *sequenceActiveTaskId_;
}

void RepeaterEngine::ClearRememberedSequenceSelectionLocked() {
    if (!activeProfileId_.has_value()) {
        return;
    }
    sequenceSelectionByProfile_.erase(*activeProfileId_);
}

bool RepeaterEngine::IsWasdProtectionActive() const {
    return wasdProtection_ && std::any_of(physicalDown_.begin(), physicalDown_.end(), IsWasdKey);
}

bool RepeaterEngine::HasSharedLongPressModifierLocked() const {
    if (IsPausedByInputLocked()) return false;
    return std::any_of(tasks_.begin(), tasks_.end(), [](const auto& task) {
        return task.config.enabled && task.config.mode == RepeatMode::LongPress
            && IsKeyboardModifierKey(task.config.key);
    });
}

bool RepeaterEngine::IsTaskActiveLocked(const TaskRuntime& task,
    std::chrono::steady_clock::time_point now) const {
    if (!toolRunning_ || suspended_.load() || !task.config.enabled || !task.config.key.IsValid()) return false;
    if (task.config.mode == RepeatMode::Hold) {
        if (holdPhysicalDown_.count(task.config.key) == 0) return false;
    } else {
        if (IsPausedByInputLocked()) return false;
        if (task.config.mode == RepeatMode::Independent
            && (!independentActiveKey_ || *independentActiveKey_ != task.config.key)) return false;
        if (task.config.mode == RepeatMode::Sequence
            && (!sequenceActiveTaskId_ || *sequenceActiveTaskId_ != task.config.id)) return false;
    }
    return (task.config.mode != RepeatMode::LongPress && task.config.key.modifiers == 0
            && !HasSharedLongPressModifierLocked())
        || (!longPressForegroundInvalidated_.load() && longPressForegroundAllowed_ && now >= longPressResumeTime_);
}

bool RepeaterEngine::IsMartialAssistEnabledLocked(const TaskRuntime& task) const {
    return mixedMode_ && task.config.enabled && task.config.mode == RepeatMode::LongPress && task.config.martialAssist;
}

bool RepeaterEngine::IsMartialAssistClosingLocked() const {
    return std::any_of(tasks_.begin(), tasks_.end(), [](const auto& task) {
        return task.martialAssist.phase == AssistPhase::Closing;
    });
}

bool RepeaterEngine::IsMartialAssistForegroundLocked(HWND window,
    std::chrono::steady_clock::time_point now) const {
    return window && window == longPressForegroundWindow_ && !longPressForegroundInvalidated_.load()
        && longPressForegroundAllowed_ && now >= longPressResumeTime_
        && !suspended_.load() && window == ::GetForegroundWindow();
}

std::optional<std::chrono::steady_clock::time_point> RepeaterEngine::MartialAssistDownSinceLocked(
    const TaskRuntime& task) const {
    const auto primary = std::find_if(outputs_.begin(), outputs_.end(), [&](const auto& output) {
        return SamePrimaryKey(output.key, task.config.key) && output.held;
    });
    if (primary == outputs_.end()) return std::nullopt;
    return driver_.ForwardedDownSince(primary->key, InputSource::Repeater);
}

void RepeaterEngine::AdvanceMartialAssistLocked(std::chrono::steady_clock::time_point now) {
    const bool paused = IsPausedByInputLocked();
    for (auto& task : tasks_) {
        auto& assist = task.martialAssist;
        const bool enabled = toolRunning_ && IsMartialAssistEnabledLocked(task);
        const auto reset = [&assist, enabled]() {
            assist = {AssistPhase::Idle, enabled ? assist.window : nullptr};
        };
        if (assist.phase == AssistPhase::CloseFailed) {
            if (!enabled) reset();
            continue;
        }
        if (assist.phase == AssistPhase::Idle) {
            const HWND target = assist.window ? assist.window : longPressForegroundWindow_;
            if (enabled && now >= assist.nextAction && IsTaskActiveLocked(task, now)
                && IsMartialAssistForegroundLocked(target, now)) {
                assist = {AssistPhase::Holding, target};
            } else if (!enabled) {
                reset();
            }
            continue;
        }

        const bool sameForeground = IsMartialAssistForegroundLocked(assist.window, now);
        if (assist.phase == AssistPhase::ManualHolding) {
            if (!enabled) reset();
            else if (!sameForeground) assist.manualHoldInterrupted = true;
            continue;
        }
        if (assist.phase == AssistPhase::Closing) {
            if (!sameForeground) {
                if (enabled) assist.phase = AssistPhase::GameRepeat;
                else reset();
            }
            continue;
        }

        if (assist.phase == AssistPhase::Holding) {
            if (!sameForeground) {
                reset();
                continue;
            }
            const auto downSince = MartialAssistDownSinceLocked(task);
            if (downSince && now >= *downSince + kMartialAssistHold) {
                assist.phase = AssistPhase::GameRepeat;
            } else if (!enabled || !IsTaskActiveLocked(task, now)) {
                reset();
            }
        }

        if (assist.phase == AssistPhase::GameRepeat && !sameForeground
            && MartialAssistDownSinceLocked(task)) {
            reset();
            continue;
        }
        if (assist.phase == AssistPhase::GameRepeat && (paused || !enabled)) {
            if (sameForeground) {
                assist = {AssistPhase::Closing, assist.window, now + kMartialAssistReleaseGap};
            } else if (!enabled) {
                reset();
            }
        }
    }
}

bool RepeaterEngine::SyncOutputsLocked() {
    std::vector<OutputState> desired;
    std::vector<KeyDescriptor> activeKeys;
    std::unordered_set<std::uint64_t> activeEncoded;
    activeKeys.reserve(tasks_.size());
    const auto now = Now();
    AdvanceMartialAssistLocked(now);
    const bool assistClosing = IsMartialAssistClosingLocked();

    for (const auto& task : tasks_) {
        const auto phase = task.martialAssist.phase;
        const bool closing = phase == AssistPhase::Closing;
        if (closing || ((phase == AssistPhase::GameRepeat || phase == AssistPhase::ManualHolding) && toolRunning_)) {
            const auto encoded = EncodeKey(task.config.key);
            if (activeEncoded.insert(encoded).second) activeKeys.push_back(task.config.key);
            continue;
        }
        if (assistClosing || !IsTaskActiveLocked(task, now)) continue;
        if (IsMartialAssistEnabledLocked(task) && phase != AssistPhase::Holding) continue;
        const auto encoded = EncodeKey(task.config.key);
        if (activeEncoded.insert(encoded).second) activeKeys.push_back(task.config.key);

        const bool longPress = task.config.mode == RepeatMode::LongPress;
        const bool assistOutput = phase == AssistPhase::Holding;
        const auto components = ExpandKeyComponents(task.config.key);
        for (std::size_t index = 0; index < components.size(); ++index) {
            const bool primary = index + 1 == components.size();
            if (primary && !longPress && !task.isDown) continue;
            const bool held = longPress || !primary;
            const bool longPressPrimary = longPress && primary;
            const auto existing = std::find_if(desired.begin(), desired.end(), [&](const auto& output) {
                return output.key == components[index];
            });
            if (existing == desired.end()) {
                desired.push_back({components[index], held, longPressPrimary, assistOutput});
            } else {
                existing->held |= held;
                existing->longPressPrimary |= longPressPrimary;
                existing->martialAssist |= assistOutput;
            }
        }
    }
    std::stable_sort(desired.begin(), desired.end(), [](const auto& left, const auto& right) {
        return IsKeyboardModifierKey(left.key) > IsKeyboardModifierKey(right.key);
    });

    const auto sameOutput = [](const auto& left, const auto& right) {
        return left.key == right.key && left.held == right.held;
    };
    // Release obsolete primary keys before changing the held modifier set.
    for (std::size_t index = outputs_.size(); index > 0; --index) {
        const auto output = outputs_[index - 1];
        if (std::any_of(desired.begin(), desired.end(), [&](const auto& item) {
                return sameOutput(item, output);
            })) continue;
        const bool traceRelease = output.traceRelease;
        const bool wasForwarded = traceRelease && driver_.ForwardedDownSince(output.key, InputSource::Repeater).has_value();
        if (!driver_.SendUp(output.key, InputSource::Repeater)) return false;
        outputs_.erase(outputs_.begin() + index - 1);
        if (output.longPressPrimary && output.martialAssist) {
            for (auto& task : tasks_) {
                auto& assist = task.martialAssist;
                if (!SamePrimaryKey(task.config.key, output.key)) continue;
                if (assist.phase == AssistPhase::Idle || assist.phase == AssistPhase::Closing) {
                    assist.nextAction = Now() + kMartialAssistReleaseGap;
                }
                if ((assist.phase == AssistPhase::Closing || assist.phase == AssistPhase::GameRepeat)
                    && !IsMartialAssistForegroundLocked(assist.window, Now())) {
                    assist = {AssistPhase::Idle, toolRunning_ ? assist.window : nullptr};
                }
            }
        }
        if (traceRelease) DiagLog::Write("long press up code=" + std::to_string(output.key.code)
            + " forwarded=" + std::to_string(wasForwarded));
    }

    if (activeEncoded != sourceActiveKeys_) {
        sourceActiveKeys_ = std::move(activeEncoded);
        driver_.SetSourceActiveKeys(InputSource::Repeater, activeKeys);
    }
    for (auto output : desired) {
        const auto existing = std::find_if(outputs_.begin(), outputs_.end(), [&](const auto& item) {
            return sameOutput(item, output);
        });
        if (existing != outputs_.end()) {
            output.traceRelease = existing->traceRelease;
            *existing = output;
            continue;
        }
        const auto source = std::find_if(tasks_.begin(), tasks_.end(), [&](const auto& task) {
            if (!SamePrimaryKey(task.config.key, output.key)) return false;
            const auto phase = task.martialAssist.phase;
            if (assistClosing || !IsTaskActiveLocked(task, now)) return false;
            if (task.config.mode == RepeatMode::LongPress) {
                return !IsMartialAssistEnabledLocked(task) || phase == AssistPhase::Holding;
            }
            return task.isDown;
        });
        if (output.martialAssist && !IsMartialAssistForegroundLocked(longPressForegroundWindow_, Now())) return false;
        if (source != tasks_.end()) {
            if (output.key.device == InputDevice::Mouse && source->config.clickPos.IsFixed()) {
                if (const auto point = ResolveClickAnchorScreenPoint(source->config.clickPos)) {
                    MoveCursorToScreenPoint(driver_, point->x, point->y);
                }
            }
        }
        if (output.martialAssist && !IsMartialAssistForegroundLocked(longPressForegroundWindow_, Now())) {
            return false;
        }
        if (source != tasks_.end()) BeforeSendLocked(source->config.key);
        const bool sent = output.held
            ? driver_.SendHeldDown(output.key, InputSource::Repeater)
            : driver_.SendDown(output.key, InputSource::Repeater);
        if (source != tasks_.end() && output.longPressPrimary) {
            const auto downSince = sent
                ? driver_.ForwardedDownSince(output.key, InputSource::Repeater) : std::nullopt;
            const bool forwarded = downSince.has_value();
            if (!source->longPressAttemptLogged || (forwarded && !source->longPressForwardedLogged)) {
                source->longPressAttemptLogged = true;
                source->longPressForwardedLogged = forwarded;
                output.traceRelease = true;
                DiagLog::Write("long press down code=" + std::to_string(output.key.code)
                    + " result=" + (forwarded ? "forwarded" : sent ? "skipped" : "failed"));
            }
        }
        if (!sent) return false;
        if (IsKeyboardModifierKey(output.key)) {
            outputs_.insert(std::find_if(outputs_.begin(), outputs_.end(), [](const auto& item) {
                return !IsKeyboardModifierKey(item.key);
            }), output);
        } else outputs_.push_back(output);
    }
    for (auto& task : tasks_) {
        auto& assist = task.martialAssist;
        if (assist.phase != AssistPhase::Closing || Now() < assist.nextAction) continue;
        if (task.config.key.device == InputDevice::Mouse && task.config.clickPos.IsFixed()
            && IsMartialAssistForegroundLocked(assist.window, Now())) {
            if (const auto point = ResolveClickAnchorScreenPoint(task.config.clickPos)) {
                MoveCursorToScreenPoint(driver_, point->x, point->y);
            }
        }
        const auto result = driver_.SendIsolatedTap(task.config.key, InputSource::Repeater, [&]() {
            return IsMartialAssistForegroundLocked(assist.window, Now());
        }, [&]() {
            BeforeSendLocked(task.config.key);
        });
        if (result == InputDriverRouter::IsolatedTapResult::Deferred) {
            assist.nextAction = Now() + kMartialAssistOwnershipPoll;
            continue;
        }
        const bool completed = result == InputDriverRouter::IsolatedTapResult::Completed;
        DiagLog::Write("martial assist isolated close code=" + std::to_string(task.config.key.code)
            + " result=" + (completed ? "completed" : "failed"));
        const bool enabled = toolRunning_ && IsMartialAssistEnabledLocked(task);
        assist = {completed || !enabled ? AssistPhase::Idle : AssistPhase::CloseFailed,
            enabled ? assist.window : nullptr, Now() + kMartialAssistReleaseGap};
        if (!completed) return false;
    }
    return true;
}

void RepeaterEngine::RefreshPhysicalHeldOutputLocked(const KeyDescriptor& key) {
    const auto modifier = ModifierMaskFromKey(key);
    if (modifier != 0) {
        for (auto& task : tasks_) {
            if ((task.config.key.modifiers & modifier) == 0 || !task.isDown) continue;
            task.isDown = false;
            task.downTime = {};
            task.lastUpTime = Now();
        }
    }
    for (std::size_t index = outputs_.size(); index > 0; --index) {
        const auto& output = outputs_[index - 1];
        const bool affectedPrimary = modifier != 0 && std::any_of(tasks_.begin(), tasks_.end(), [&](const auto& task) {
            return (task.config.key.modifiers & modifier) != 0 && SamePrimaryKey(task.config.key, output.key);
        });
        const bool affectedHeld = output.held && (SamePrimaryKey(output.key, key)
            || (modifier != 0 && ModifierMaskFromKey(output.key) == modifier));
        if (!affectedPrimary && !affectedHeld) continue;
        if (!driver_.SendUp(output.key, InputSource::Repeater)) return;
        outputs_.erase(outputs_.begin() + index - 1);
        if (output.traceRelease) {
            DiagLog::Write("long press reset after physical release code=" + std::to_string(output.key.code));
            for (auto& task : tasks_) {
                if (task.config.mode == RepeatMode::LongPress && SamePrimaryKey(task.config.key, output.key)) {
                    task.longPressForwardedLogged = false;
                }
            }
        }
    }
}

void RepeaterEngine::ReleaseAll() {
    std::lock_guard lock(mutex_);
    running_.store(false);
    toolRunning_ = false;
    physicalDown_.clear();
    ignoreCount_.clear();
    holdPhysicalDown_.clear();
    holdUpIgnoreCount_.clear();
    pauseKeysDown_.clear();
    independentDown_.clear();
    independentDownIgnoreCount_.clear();
    independentUpIgnoreCount_.clear();
    independentActiveKey_.reset();
    sequenceSwitchDown_ = false;
    sequenceActiveTaskId_.reset();
    ResetAllLocked();
    for (auto& task : tasks_) task.martialAssist = {};
    SyncOutputsLocked();
    outputs_.clear();
    sourceActiveKeys_.clear();
    driver_.ReleaseSource(InputSource::Repeater);
}

bool RepeaterEngine::HasActiveHoldKeys() const {
    std::lock_guard lock(mutex_);
    return !holdPhysicalDown_.empty();
}

// ── bindings ─────────────────────────────
void RepeaterEngine::UpdateBindings(std::uint64_t profileId, const std::vector<KeyBinding>& bindings, bool rememberSequencePosition) {
    std::lock_guard lock(mutex_);
    waiter_.Wake();

    const bool sameProfile = activeProfileId_ == profileId;
    if (!sameProfile) {
        activeProfileId_ = profileId;
        sequenceActiveTaskId_.reset();
    }

    std::vector<TaskRuntime> updated;
    rememberSequencePosition_ = rememberSequencePosition;
    if (!rememberSequencePosition_) ClearRememberedSequenceSelectionLocked();
    updated.reserve(bindings.size());

    for (auto& b : bindings) {
        TaskRuntime t;
        t.config = {
            b.id,
            b.key,
            b.mode,
            b.delayMs,
            b.enabled,
            b.suppressMixedMode,
            b.key.device == InputDevice::Mouse ? b.clickPos : ClickPosition{},
            b.martialAssist,
        };
        t.config.preventAccidentalStop = b.preventAccidentalStop;

        auto it = std::find_if(tasks_.begin(), tasks_.end(),
            [&](auto& x) { return x.config.id == b.id; });

        if (it != tasks_.end()) {
            t.lastUpTime = it->lastUpTime;
            if (it->config.mode == t.config.mode && it->config.key == t.config.key && t.config.enabled) {
                t.isDown = it->isDown;
                t.downTime = it->downTime;
            }
            if (sameProfile && it->config.key == t.config.key && it->config.mode == t.config.mode) {
                if (it->config.martialAssist) t.martialAssist = it->martialAssist;
                t.longPressAttemptLogged = it->longPressAttemptLogged;
                t.longPressForwardedLogged = it->longPressForwardedLogged;
            }
        }

        updated.push_back(t);
    }

    tasks_ = std::move(updated);
    NormalizeIndependentSelectionLocked();
    NormalizeSequenceSelectionLocked();
    SyncOutputsLocked();
}

RepeaterStatus RepeaterEngine::GetStatus() const {
    std::lock_guard lock(mutex_);
    RepeaterStatus status;
    status.running = toolRunning_;
    if (!toolRunning_) return status;
    status.paused = IsPauseActive();
    status.holdPriorityActive = mixedMode_ && IsHoldActive();
    status.independentKey = independentActiveKey_;
    status.sequenceTaskId = sequenceActiveTaskId_;
    for (const auto& task : tasks_) {
        if (task.config.enabled && task.config.mode == RepeatMode::Hold
            && holdPhysicalDown_.count(task.config.key) > 0) {
            status.holdKeys.push_back(task.config.key);
        }
    }
    return status;
}

void RepeaterEngine::ForgetProfileRuntimeState(std::uint64_t profileId) {
    std::lock_guard lock(mutex_);
    waiter_.Wake();

    sequenceSelectionByProfile_.erase(profileId);
    if (activeProfileId_.has_value() && *activeProfileId_ == profileId) {
        activeProfileId_.reset();
        sequenceActiveTaskId_.reset();
        SyncOutputsLocked();
    }
}



// ── Worker（状态机版：down/up 分离发送） ─────────────
void RepeaterEngine::WorkerLoop() {
    while (!exit_.load()) {
        std::optional<std::chrono::steady_clock::time_point> nextWake;
        {
            std::lock_guard lock(mutex_);
            NormalizeIndependentSelectionLocked();
            NormalizeSequenceSelectionLocked();
            const bool wasdProtectionActive = IsWasdProtectionActive();
            const bool assistWasClosing = IsMartialAssistClosingLocked();
            std::vector<TaskRuntime*> presses;
            std::vector<TaskRuntime*> releases;
            const auto schedule = [&nextWake](const auto deadline) {
                if (!nextWake || deadline < *nextWake) nextWake = deadline;
            };

            for (auto& t : tasks_) {
                const auto now = Now();
                if (assistWasClosing || !IsTaskActiveLocked(t, now)) {
                    if (t.isDown) {
                        t.isDown = false;
                        releases.push_back(&t);
                    }
                    if (now < longPressResumeTime_ && IsTaskActiveLocked(t, longPressResumeTime_)) {
                        schedule(longPressResumeTime_);
                    }
                    continue;
                }
                if (t.config.mode == RepeatMode::LongPress) continue;

                if (t.isDown) {
                    if (now - t.downTime >= std::chrono::milliseconds(kInputPulseWidthMs)) {
                        t.isDown = false;
                        releases.push_back(&t);
                    }
                    continue;
                }

                int interval = std::max(kMinDelayMs, t.config.intervalMs);
                if (wasdProtectionActive && IsToggleLikeRepeatMode(t.config.mode)) {
                    interval = std::max(interval, kWasdProtectionMinIntervalMs);
                }
                if (t.lastUpTime == std::chrono::steady_clock::time_point{}
                    || now - t.lastUpTime >= std::chrono::milliseconds(interval)) {
                    presses.push_back(&t);
                    t.isDown = true;
                    t.downTime = now;
                }
            }

            const bool outputsSynced = SyncOutputsLocked();
            const auto syncedAt = Now();
            if (!outputsSynced) schedule(syncedAt + std::chrono::milliseconds(kDefaultDelayMs));
            const bool assistClosing = IsMartialAssistClosingLocked();
            if (assistWasClosing && !assistClosing) schedule(Now());
            for (auto* task : releases) task->lastUpTime = Now();
            for (auto* task : presses) {
                const bool sent = !assistClosing && std::any_of(outputs_.begin(), outputs_.end(), [&](const auto& output) {
                    return SamePrimaryKey(output.key, task->config.key);
                });
                if (sent) task->downTime = Now();
                else {
                    task->isDown = false;
                    if (!assistClosing) task->lastUpTime = Now();
                }
            }

            for (const auto& task : tasks_) {
                const auto& assist = task.martialAssist;
                if (assist.phase == AssistPhase::Holding) {
                    const auto since = MartialAssistDownSinceLocked(task);
                    schedule(since ? *since + kMartialAssistHold : syncedAt + kMartialAssistOwnershipPoll);
                } else if (assist.phase == AssistPhase::Closing) {
                    if (outputsSynced || assist.nextAction > syncedAt) schedule(assist.nextAction);
                } else if (assist.phase == AssistPhase::Idle && Now() < assist.nextAction
                    && IsMartialAssistEnabledLocked(task) && IsTaskActiveLocked(task, Now())) {
                    schedule(assist.nextAction);
                }
                if (assistClosing || task.config.mode == RepeatMode::LongPress || !IsTaskActiveLocked(task, Now())) continue;

                if (task.isDown) {
                    schedule(task.downTime + std::chrono::milliseconds(kInputPulseWidthMs));
                    continue;
                }
                if (task.lastUpTime == std::chrono::steady_clock::time_point{}) continue;

                int interval = std::max(kMinDelayMs, task.config.intervalMs);
                if (wasdProtectionActive && IsToggleLikeRepeatMode(task.config.mode)) {
                    interval = std::max(interval, kWasdProtectionMinIntervalMs);
                }
                schedule(task.lastUpTime + std::chrono::milliseconds(interval));
            }
        }
        waiter_.WaitUntil(nextWake);
    }
}
void RepeaterEngine::ResetAllLocked() {
    for (auto& t : tasks_) {
        t.lastUpTime = {};
        t.isDown = false;
        t.downTime = {};
    }
}

// ── time ─────────────────────────────
std::chrono::steady_clock::time_point RepeaterEngine::Now() {
    return std::chrono::steady_clock::now();
}

