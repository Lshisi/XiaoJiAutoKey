#include "RepeaterEngine.h"

#include "ClickPositioning.h"

#include <algorithm>

namespace {
constexpr int kWasdProtectionMinIntervalMs = 35;

std::uint64_t EncodeKey(const KeyDescriptor& key) {
    return (static_cast<std::uint64_t>(key.device == InputDevice::Mouse ? 1 : 0) << 32) | key.code;
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

RepeaterEngine::RepeaterEngine(InputDriver& driver)
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
    waiter_.Wake();
    std::lock_guard lock(mutex_);
    mixedMode_ = enabled;
}

void RepeaterEngine::SetWasdProtection(bool enabled) {
    waiter_.Wake();
    std::lock_guard lock(mutex_);
    wasdProtection_ = enabled;
}

void RepeaterEngine::SetSequenceSwitchKey(std::optional<KeyDescriptor> key) {
    waiter_.Wake();
    std::lock_guard lock(mutex_);
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
    waiter_.Wake();
    std::lock_guard lock(mutex_);
    pauseKeys_ = keys;
    pauseKeysDown_.clear();
}

void RepeaterEngine::SetSuspended(bool suspended) {
    waiter_.Wake();
    suspended_.store(suspended);
}

bool RepeaterEngine::IsPauseActive() const {
    return !pauseKeysDown_.empty();
}

// ── 启停 ─────────────────────────────
void RepeaterEngine::SetToolRunning(bool running) {
    waiter_.Wake();
    std::lock_guard lock(mutex_);
    ++runGeneration_;
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

    for (auto& t : tasks_) {
        t.lastUpTime = {};
        t.isDown = false;
        t.downTime = {};
    }

    if (running) {
        NormalizeSequenceSelectionLocked();
    }

    SyncSourceActiveKeysLocked();
}

// ── 物理输入状态追踪 ─────────────
void RepeaterEngine::OnPhysicalDown(const KeyDescriptor& key) {
    waiter_.Wake();
    std::lock_guard lock(mutex_);
    const auto encoded = EncodeKey(key);
    const bool independentBinding = IsIndependentBinding(key);

    if (sequenceSwitchKey_.has_value() && *sequenceSwitchKey_ == key) {
        if (!sequenceSwitchDown_) {
            sequenceSwitchDown_ = true;
            AdvanceSequenceSelectionLocked();
            SyncSourceActiveKeysLocked();
        }
        return;
    }

    // 暂停键追踪
    for (const auto& pk : pauseKeys_) {
        if (pk == key) {
            pauseKeysDown_.insert(key);
            break;
        }
    }

    auto it = ignoreCount_.find(encoded);

    if (independentBinding) {
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
        if (independentActiveKey_.has_value() && *independentActiveKey_ == key) {
            independentActiveKey_.reset();
        } else {
            independentActiveKey_ = key;
            ResetIndependentTaskTimingLocked(key);
        }
        SyncSourceActiveKeysLocked();
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
    SyncSourceActiveKeysLocked();
}

void RepeaterEngine::OnPhysicalUp(const KeyDescriptor& key) {
    waiter_.Wake();
    std::lock_guard lock(mutex_);
    const auto encoded = EncodeKey(key);
    const bool independentBinding = IsIndependentBinding(key);

    if (sequenceSwitchKey_.has_value() && *sequenceSwitchKey_ == key) {
        sequenceSwitchDown_ = false;
        return;
    }

    // 暂停键追踪
    pauseKeysDown_.erase(key);

    if (independentBinding) {
        auto independentIgnoreIt = independentUpIgnoreCount_.find(encoded);
        if (independentIgnoreIt != independentUpIgnoreCount_.end() && independentIgnoreIt->second > 0) {
            --independentIgnoreIt->second;
            return;
        }
        independentDown_.erase(key);
        physicalDown_.erase(key);
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
            holdPhysicalDown_.erase(key);
        }
    }

    // 原有的physicalDown_追踪
    auto it = ignoreCount_.find(encoded);
    if (it != ignoreCount_.end() && it->second > 0) {
        --it->second;
        return;
    }
    physicalDown_.erase(key);
    SyncSourceActiveKeysLocked();
}

void RepeaterEngine::BeforeSendLocked(const KeyDescriptor& key) {
    if ((key.device == InputDevice::Keyboard && !selfInjectedKeyboardVisible_) ||
        (key.device == InputDevice::Mouse && !selfInjectedMouseVisible_)) {
        return;
    }

    const auto encoded = EncodeKey(key);
    if (IsIndependentBinding(key)) {
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
        return t.config.enabled && t.config.mode == RepeatMode::Hold && t.config.key == key;
    });
}

bool RepeaterEngine::IsIndependentBinding(const KeyDescriptor& key) const {
    return std::any_of(tasks_.begin(), tasks_.end(), [&](const TaskRuntime& t) {
        return t.config.enabled && t.config.mode == RepeatMode::Independent && t.config.key == key;
    });
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
                driver_.SendUp(currentTask.config.key, InputSource::Repeater);
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

void RepeaterEngine::SyncSourceActiveKeysLocked() {
    std::vector<KeyDescriptor> activeKeys;
    std::unordered_set<std::uint64_t> activeEncoded;
    activeKeys.reserve(tasks_.size());

    for (const auto& task : tasks_) {
        if (!task.config.enabled || !task.config.key.IsValid()) continue;

        bool active = false;
        switch (task.config.mode) {
        case RepeatMode::Hold:
            active = toolRunning_ && holdPhysicalDown_.count(task.config.key) > 0;
            break;
        case RepeatMode::Independent:
            active = toolRunning_
                && independentActiveKey_.has_value()
                && *independentActiveKey_ == task.config.key;
            break;
        case RepeatMode::Sequence:
            active = toolRunning_
                && sequenceActiveTaskId_.has_value()
                && *sequenceActiveTaskId_ == task.config.id;
            break;
        case RepeatMode::Toggle:
            active = toolRunning_;
            break;
        }

        const auto encoded = EncodeKey(task.config.key);
        if (active && activeEncoded.insert(encoded).second) {
            activeKeys.push_back(task.config.key);
        }
    }

    if (activeEncoded == sourceActiveKeys_) return;
    sourceActiveKeys_ = std::move(activeEncoded);
    driver_.SetSourceActiveKeys(InputSource::Repeater, activeKeys);
}

void RepeaterEngine::ReleaseAll() {
    std::lock_guard lock(mutex_);
    ++runGeneration_;
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
    sourceActiveKeys_.clear();
    driver_.ReleaseSource(InputSource::Repeater);
}

bool RepeaterEngine::HasActiveHoldKeys() const {
    std::lock_guard lock(mutex_);
    return !holdPhysicalDown_.empty();
}

// ── bindings ─────────────────────────────
void RepeaterEngine::UpdateBindings(std::uint64_t profileId, const std::vector<KeyBinding>& bindings, bool rememberSequencePosition) {
    waiter_.Wake();
    std::lock_guard lock(mutex_);

    if (!activeProfileId_.has_value() || *activeProfileId_ != profileId) {
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
        };

        auto it = std::find_if(tasks_.begin(), tasks_.end(),
            [&](auto& x) { return x.config.id == b.id; });

        if (it != tasks_.end()) {
            t.lastUpTime = it->lastUpTime;
        }

        updated.push_back(t);
    }

    tasks_ = std::move(updated);
    NormalizeIndependentSelectionLocked();
    NormalizeSequenceSelectionLocked();
    SyncSourceActiveKeysLocked();
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
    waiter_.Wake();
    std::lock_guard lock(mutex_);

    sequenceSelectionByProfile_.erase(profileId);
    if (activeProfileId_.has_value() && *activeProfileId_ == profileId) {
        activeProfileId_.reset();
        sequenceActiveTaskId_.reset();
        SyncSourceActiveKeysLocked();
    }
}



// ── Worker（状态机版：down/up 分离发送） ─────────────
void RepeaterEngine::WorkerLoop() {
    while (!exit_.load()) {
        std::vector<KeyDescriptor> releases;
        std::vector<std::pair<KeyDescriptor, ClickPosition>> presses;
        std::uint64_t batchGeneration = 0;
        std::optional<std::chrono::steady_clock::time_point> nextWake;
        {
            std::lock_guard lock(mutex_);
            batchGeneration = runGeneration_.load();

            NormalizeIndependentSelectionLocked();
            NormalizeSequenceSelectionLocked();

            const bool holdActive = IsHoldActive();
            const bool pauseActive = IsPauseActive();
            const bool suspended = suspended_.load();
            const bool blocked = pauseActive || suspended;
            const bool wasdProtectionActive = IsWasdProtectionActive();
            const std::optional<KeyDescriptor> selectedIndependentKey = independentActiveKey_;
            const std::optional<std::uint64_t> selectedSequenceTaskId = sequenceActiveTaskId_;
            const auto queueRelease = [&](TaskRuntime& task) {
                releases.push_back(task.config.key);
                task.isDown = false;
                task.lastUpTime = Now();
            };

            for (auto& t : tasks_) {
                if (!t.config.enabled) continue;

                const bool isHoldMode = t.config.mode == RepeatMode::Hold;
                const bool isIndependentMode = t.config.mode == RepeatMode::Independent;
                const bool isSequenceMode = t.config.mode == RepeatMode::Sequence;
                const bool isToggleLikeMode = IsToggleLikeRepeatMode(t.config.mode);

                int interval = std::max(kMinDelayMs, t.config.intervalMs);
                if (wasdProtectionActive && isToggleLikeMode) {
                    interval = std::max(interval, kWasdProtectionMinIntervalMs);
                }

                if (isHoldMode) {
                    const bool isDown = holdPhysicalDown_.count(t.config.key) > 0 && !suspended;
                    if (!isDown) {
                        if (t.isDown) {
                            queueRelease(t);
                        }
                        continue;
                    }
                } else if (isIndependentMode) {
                    const bool selected = selectedIndependentKey.has_value() && *selectedIndependentKey == t.config.key;
                    if (!toolRunning_ || !selected) {
                        if (t.isDown) {
                            queueRelease(t);
                        }
                        continue;
                    }
                    if ((mixedMode_ && holdActive) || blocked) {
                        if (t.isDown) {
                            queueRelease(t);
                        }
                        continue;
                    }
                } else if (isSequenceMode) {
                    const bool selected = selectedSequenceTaskId.has_value()
                        && *selectedSequenceTaskId == t.config.id;
                    if (!toolRunning_ || !selected) {
                        if (t.isDown) {
                            queueRelease(t);
                        }
                        continue;
                    }
                    if ((mixedMode_ && holdActive) || blocked) {
                        if (t.isDown) {
                            queueRelease(t);
                        }
                        continue;
                    }
                } else {
                    if (!toolRunning_) {
                        if (t.isDown) {
                            queueRelease(t);
                        }
                        continue;
                    }
                    if ((mixedMode_ && holdActive) || blocked) {
                        if (t.isDown) {
                            queueRelease(t);
                        }
                        continue;
                    }
                }

                const auto now = Now();

                if (t.isDown) {
                    if (now - t.downTime >= std::chrono::milliseconds(kInputPulseWidthMs)) {
                        queueRelease(t);
                    }
                    continue;
                }

                if (t.lastUpTime == std::chrono::steady_clock::time_point{}
                    || now - t.lastUpTime >= std::chrono::milliseconds(interval)) {
                    presses.emplace_back(t.config.key, t.config.clickPos);
                    t.isDown = true;
                    t.downTime = now;
                }
            }

            for (const auto& [key, clickPos] : presses) {
                BeforeSendLocked(key);
            }

            if (batchGeneration == runGeneration_.load()) {
                for (const auto& key : releases) {
                    driver_.SendUp(key, InputSource::Repeater);
                }
                for (const auto& [key, clickPos] : presses) {
                    if (key.device == InputDevice::Mouse && clickPos.IsFixed()) {
                        if (const auto point = ResolveClickAnchorScreenPoint(clickPos)) {
                            MoveCursorToScreenPoint(driver_, point->x, point->y);
                        }
                    }
                    driver_.SendDown(key, InputSource::Repeater);
                }
            }

            for (const auto& task : tasks_) {
                if (!task.config.enabled) continue;

                if (task.isDown) {
                    const auto deadline = task.downTime + std::chrono::milliseconds(kInputPulseWidthMs);
                    nextWake = !nextWake || deadline < *nextWake ? deadline : nextWake;
                    continue;
                }

                const bool independentSelected = task.config.mode != RepeatMode::Independent
                    || (independentActiveKey_.has_value() && *independentActiveKey_ == task.config.key);
                const bool sequenceSelected = task.config.mode != RepeatMode::Sequence
                    || (sequenceActiveTaskId_.has_value() && *sequenceActiveTaskId_ == task.config.id);
                const bool canRun = task.config.mode == RepeatMode::Hold
                    ? holdPhysicalDown_.count(task.config.key) > 0
                    : toolRunning_ && independentSelected && sequenceSelected
                        && !(mixedMode_ && IsHoldActive()) && !IsPauseActive();
                if (!canRun || task.lastUpTime == std::chrono::steady_clock::time_point{}) continue;

                int interval = std::max(kMinDelayMs, task.config.intervalMs);
                if (IsWasdProtectionActive() && IsToggleLikeRepeatMode(task.config.mode)) {
                    interval = std::max(interval, kWasdProtectionMinIntervalMs);
                }
                const auto deadline = task.lastUpTime + std::chrono::milliseconds(interval);
                nextWake = !nextWake || deadline < *nextWake ? deadline : nextWake;
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

