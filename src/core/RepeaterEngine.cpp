#include "RepeaterEngine.h"

#include <algorithm>

namespace {
std::uint64_t EncodeKey(const KeyDescriptor& key) {
    return (static_cast<std::uint64_t>(key.device == InputDevice::Mouse ? 1 : 0) << 32) | key.code;
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
        toolRunning_ = running;
        running_.store(running);

        physicalDown_.clear();
        holdPhysicalDown_.clear();
        holdUpIgnoreCount_.clear();
        pauseKeysDown_.clear();

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

    // 暂停键追踪
    for (const auto& pk : pauseKeys_) {
        if (pk == key) {
            pauseKeysDown_.insert(key);
            break;
        }
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
    auto it = ignoreCount_.find(encoded);
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
}

void RepeaterEngine::OnPhysicalUp(const KeyDescriptor& key) {
    std::lock_guard lock(mutex_);
    const auto encoded = EncodeKey(key);

    // 暂停键追踪
    pauseKeysDown_.erase(key);

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

void RepeaterEngine::BeforeSend(const KeyDescriptor& key) {
    std::lock_guard lock(mutex_);
    const auto encoded = EncodeKey(key);
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

bool RepeaterEngine::IsHoldActive() const {
    return std::any_of(tasks_.begin(), tasks_.end(), [&](const TaskRuntime& t) {
        return t.config.enabled
            && t.config.mode == RepeatMode::Hold
            && !t.config.suppressMixedMode
            && holdPhysicalDown_.count(t.config.key) > 0;
    });
}

void RepeaterEngine::ReleaseAll() {
    {
        std::lock_guard lock(mutex_);
        physicalDown_.clear();
        holdPhysicalDown_.clear();
        holdUpIgnoreCount_.clear();
        pauseKeysDown_.clear();
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
}



// ── Worker（状态机版：down/up 分离发送） ─────────────
void RepeaterEngine::WorkerLoop() {
    while (!exit_.load()) {
        // 收集需要发送的事件
        std::vector<std::pair<KeyDescriptor, bool>> toSend;  // key, isDown
        {
            std::lock_guard lock(mutex_);

            const bool holdActive = IsHoldActive();
            const bool pauseActive = IsPauseActive();

            for (auto& t : tasks_) {
                if (!t.config.enabled) continue;

                const int interval = std::max(1, t.config.intervalMs);
                const bool isDown = physicalDown_.count(t.config.key) > 0;

                // Hold模式：必须按住才发
                if (t.config.mode == RepeatMode::Hold && !isDown) {
                    // 如果松开了但还有 pending 的 down，立即发 up
                    if (t.isDown) {
                        toSend.push_back({t.config.key, false});
                        t.isDown = false;
                    }
                    continue;
                }

                // Toggle模式：仅在混合模式开启时被Hold激活压制，或暂停键按住时压制
                if (t.config.mode == RepeatMode::Toggle) {
                    if (!toolRunning_) {
                        if (t.isDown) {
                            toSend.push_back({t.config.key, false});
                            t.isDown = false;
                        }
                        continue;
                    }
                    if ((mixedMode_ && holdActive) || pauseActive) {
                        if (t.isDown) {
                            toSend.push_back({t.config.key, false});
                            t.isDown = false;
                        }
                        continue;
                    }
                }

                const auto now = NowMs();

                // 如果当前处于 down 状态，检查是否该发 up
                if (t.isDown) {
                    // hold 时间 = min(interval / 3, 30)，最少 2ms
                    const int holdMs = std::max(2, std::min(interval / 3, 30));
                    if (now - t.downTime >= (ULONGLONG)holdMs) {
                        toSend.push_back({t.config.key, false});
                        t.isDown = false;
                        t.lastFire = now;
                    }
                    continue;
                }

                // 检查是否该发 down
                if (t.lastFire == 0 || now - t.lastFire >= (ULONGLONG)interval) {
                    toSend.push_back({t.config.key, true});
                    t.isDown = true;
                    t.downTime = now;
                }
            }
        }

        // 在锁外发送事件
        for (const auto& [key, down] : toSend) {
            if (down) {
                BeforeSend(key);
                driver_.SendDown(key, InputSource::Injected);
            } else {
                driver_.SendUp(key, InputSource::Injected);
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

// ── reset ─────────────────────────────
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
