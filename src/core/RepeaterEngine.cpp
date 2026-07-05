#include "RepeaterEngine.h"

#include <algorithm>

namespace {
constexpr int kWasdProtectionMinIntervalMs = 50;

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

RepeaterEngine::RepeaterEngine(DdDriver& driver)
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
    if (worker_.joinable()) worker_.join();
    ReleaseAll();
}

bool RepeaterEngine::IsToolRunning() const noexcept {
    return running_.load();
}

void RepeaterEngine::SetMixedMode(bool enabled) {
    std::lock_guard lock(mutex_);
    mixedMode_ = enabled;
}

void RepeaterEngine::SetWasdProtection(bool enabled) {
    std::lock_guard lock(mutex_);
    wasdProtection_ = enabled;
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

void RepeaterEngine::SetPauseKeys(const std::vector<KeyDescriptor>& keys) {
    std::lock_guard lock(mutex_);
    pauseKeys_ = keys;
    pauseKeysDown_.clear();
}

bool RepeaterEngine::IsPauseActive() const {
    return !pauseKeysDown_.empty();
}

// ── 启停 ─────────────────────────────
void RepeaterEngine::SetToolRunning(bool running) {
    {
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

        for (auto& t : tasks_) {
            t.lastFire = 0;
            t.isDown = false;
            t.downTime = 0;
        }
    }

    // ⭐ 关键修复：清空driver层所有状态，防止状态污染
    if (!running) {
        driver_.ReleaseAll();
        driver_.ClearAllStates();
    }
}

// ── 物理输入状态追踪 ─────────────
void RepeaterEngine::OnPhysicalDown(const KeyDescriptor& key) {
    std::lock_guard lock(mutex_);
    const auto encoded = EncodeKey(key);
    const bool independentBinding = IsIndependentBinding(key);

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

    if (independentBinding) {
        if (independentActiveKey_.has_value() && *independentActiveKey_ == key) {
            independentActiveKey_.reset();
        } else {
            independentActiveKey_ = key;
        }
        ResetIndependentTaskTimingLocked(key);
    }
}

void RepeaterEngine::OnPhysicalUp(const KeyDescriptor& key) {
    std::lock_guard lock(mutex_);
    const auto encoded = EncodeKey(key);
    const bool independentBinding = IsIndependentBinding(key);

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
}

void RepeaterEngine::BeforeSendLocked(const KeyDescriptor& key) {
    if (key.device == InputDevice::Keyboard && !selfInjectedKeyboardVisible_) {
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
            t.lastFire = 0;
        }
    }
}

bool RepeaterEngine::IsWasdProtectionActive() const {
    return wasdProtection_ && std::any_of(physicalDown_.begin(), physicalDown_.end(), IsWasdKey);
}

void RepeaterEngine::ReleaseAll() {
    {
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
        ResetAllLocked();
    }

    driver_.ReleaseAll();
}

bool RepeaterEngine::HasActiveHoldKeys() const {
    std::lock_guard lock(mutex_);
    return !holdPhysicalDown_.empty();
}

// ── bindings ─────────────────────────────
void RepeaterEngine::UpdateBindings(const std::vector<KeyBinding>& bindings) {
    std::lock_guard lock(mutex_);

    std::vector<TaskRuntime> updated;
    updated.reserve(bindings.size());

    for (auto& b : bindings) {
        TaskRuntime t;
        t.config = {
            b.id,
            b.key,
            b.mode,
            b.delayMs,
            b.enabled,
            b.suppressMixedMode
        };

        auto it = std::find_if(tasks_.begin(), tasks_.end(),
            [&](auto& x) { return x.config.id == b.id; });

        if (it != tasks_.end()) {
            t.lastFire = it->lastFire;
        }

        updated.push_back(t);
    }

    tasks_ = std::move(updated);
    NormalizeIndependentSelectionLocked();
}



// ── Worker（状态机版：down/up 分离发送） ─────────────
void RepeaterEngine::WorkerLoop() {
    while (!exit_.load()) {
        std::vector<KeyDescriptor> releases;
        std::vector<KeyDescriptor> presses;
        std::uint64_t batchGeneration = 0;
        {
            std::lock_guard lock(mutex_);
            batchGeneration = runGeneration_.load();

            NormalizeIndependentSelectionLocked();

            const bool holdActive = IsHoldActive();
            const bool pauseActive = IsPauseActive();
            const bool wasdProtectionActive = IsWasdProtectionActive();
            const std::optional<KeyDescriptor> selectedIndependentKey = independentActiveKey_;

            for (auto& t : tasks_) {
                if (!t.config.enabled) continue;

                const bool isHoldMode = t.config.mode == RepeatMode::Hold;
                const bool isIndependentMode = t.config.mode == RepeatMode::Independent;
                const bool isToggleLikeMode = IsToggleLikeRepeatMode(t.config.mode);

                int interval = std::max(1, t.config.intervalMs);
                if (wasdProtectionActive && isToggleLikeMode) {
                    interval = std::max(interval, kWasdProtectionMinIntervalMs);
                }

                if (isHoldMode) {
                    const bool isDown = holdPhysicalDown_.count(t.config.key) > 0;
                    if (!isDown) {
                        if (t.isDown) {
                            releases.push_back(t.config.key);
                            t.isDown = false;
                        }
                        continue;
                    }
                } else if (isIndependentMode) {
                    const bool selected = selectedIndependentKey.has_value() && *selectedIndependentKey == t.config.key;
                    if (!toolRunning_ || !selected) {
                        if (t.isDown) {
                            releases.push_back(t.config.key);
                            t.isDown = false;
                        }
                        continue;
                    }
                    if ((mixedMode_ && holdActive) || pauseActive) {
                        if (t.isDown) {
                            releases.push_back(t.config.key);
                            t.isDown = false;
                        }
                        continue;
                    }
                } else {
                    if (!toolRunning_) {
                        if (t.isDown) {
                            releases.push_back(t.config.key);
                            t.isDown = false;
                        }
                        continue;
                    }
                    if ((mixedMode_ && holdActive) || pauseActive) {
                        if (t.isDown) {
                            releases.push_back(t.config.key);
                            t.isDown = false;
                        }
                        continue;
                    }
                }

                const auto now = NowMs();

                if (isIndependentMode) {
                    if (t.isDown) {
                        const int holdMs = std::max(2, std::min(interval / 8, 6));
                        if (now - t.downTime >= static_cast<ULONGLONG>(holdMs)) {
                            releases.push_back(t.config.key);
                            t.isDown = false;
                            t.lastFire = now;
                        }
                        continue;
                    }

                    if (t.lastFire == 0 || now - t.lastFire >= static_cast<ULONGLONG>(interval)) {
                        presses.push_back(t.config.key);
                        t.isDown = true;
                        t.downTime = now;
                    }
                    continue;
                }

                if (t.isDown) {
                    const int holdMs = std::max(2, std::min(interval / 3, 30));
                    if (now - t.downTime >= static_cast<ULONGLONG>(holdMs)) {
                        releases.push_back(t.config.key);
                        t.isDown = false;
                        t.lastFire = now;
                    }
                    continue;
                }

                if (t.lastFire == 0 || now - t.lastFire >= static_cast<ULONGLONG>(interval)) {
                    presses.push_back(t.config.key);
                    t.isDown = true;
                    t.downTime = now;
                }
            }

            for (const auto& key : presses) {
                BeforeSendLocked(key);
            }

            if (batchGeneration == runGeneration_.load()) {
                for (const auto& key : releases) {
                    driver_.SendUp(key, InputSource::Injected);
                }
                for (const auto& key : presses) {
                    driver_.SendDown(key, InputSource::Injected);
                }
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}
void RepeaterEngine::ResetAllLocked() {
    for (auto& t : tasks_) {
        t.lastFire = 0;
        t.isDown = false;
        t.downTime = 0;
    }
}

// ── time ─────────────────────────────
ULONGLONG RepeaterEngine::NowMs() {
    return ::GetTickCount64();
}

