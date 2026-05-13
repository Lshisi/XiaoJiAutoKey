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

// ── 启停 ─────────────────────────────
void RepeaterEngine::SetToolRunning(bool running) {
    {
        std::lock_guard lock(mutex_);
        toolRunning_ = running;
        running_.store(running);

        physicalDown_.clear();
        holdPhysicalDown_.clear();
        holdUpIgnoreCount_.clear();

        for (auto& t : tasks_) {
            t.lastFire = 0;
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
    return !holdPhysicalDown_.empty();
}

void RepeaterEngine::ReleaseAll() {
    {
        std::lock_guard lock(mutex_);
        physicalDown_.clear();
        holdPhysicalDown_.clear();
        holdUpIgnoreCount_.clear();
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
            b.enabled
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



// ── Worker（tick驱动版） ─────────────
void RepeaterEngine::WorkerLoop() {
    while (!exit_.load()) {
        // 收集需要发送的按键
        std::vector<KeyDescriptor> toSend;
        {
            std::lock_guard lock(mutex_);

            // ⭐ 使用独立的holdPhysicalDown_来判断Hold是否激活（不受ignoreCount/键盘重复影响）
            const bool holdActive = IsHoldActive();

            for (auto& t : tasks_) {
                if (!t.config.enabled) continue;

                const int interval = std::max(1, t.config.intervalMs);
                // 使用Hook维护的physicalDown_（已通过per-key时间戳过滤了SendTap事件）
                const bool isDown = physicalDown_.count(t.config.key) > 0;

                // Hold模式：必须按住才发
                if (t.config.mode == RepeatMode::Hold && !isDown) continue;

                // ⭐ 关键：Toggle模式在Hold激活时被压制
                if (t.config.mode == RepeatMode::Toggle) {
                    if (!toolRunning_) continue;
                    if (holdActive) continue;  // Hold优先级压制Toggle
                }

                const auto now = NowMs();
                if (t.lastFire == 0 || now - t.lastFire >= (ULONGLONG)interval) {
                    t.lastFire = now;
                    toSend.push_back(t.config.key);
                }
            }
        }

        // 在锁外发送按键，避免死锁
        for (const auto& key : toSend) {
            BeforeSend(key);  // ⭐ SendTap前设置计数器，Hook会忽略接下来的2个事件
            driver_.SendTap(key, InputSource::Injected);
        }

        // 短暂休眠，避免CPU占用过高
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

// ── reset ─────────────────────────────
void RepeaterEngine::ResetAllLocked() {
    for (auto& t : tasks_) {
        t.lastFire = 0;
    }
}

// ── time ─────────────────────────────
ULONGLONG RepeaterEngine::NowMs() {
    return ::GetTickCount64();
}
