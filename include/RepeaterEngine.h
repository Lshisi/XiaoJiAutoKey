#pragma once

#include "AppTypes.h"
#include "DdDriver.h"

#include <Windows.h>

#include <atomic>
#include <cstdint>
#include <mutex>
#include <thread>
#include <unordered_set>
#include <vector>



// ── Key hash ─────────────────────────────
struct KeyDescriptorHash {
    std::size_t operator()(const KeyDescriptor& k) const noexcept {
        return (static_cast<std::uint64_t>(k.device) << 32) ^ k.code;
    }
};

// ── 配置 ────────────────────────────────
struct TaskConfig {
    std::uint64_t id = 0;
    KeyDescriptor key;
    RepeatMode mode = RepeatMode::Toggle;
    int intervalMs = 100;
    bool enabled = true;
};

// ── Runtime（已移除 schedule，避免时间漂移） ─────────────
struct TaskRuntime {
    TaskConfig config;
    ULONGLONG lastFire = 0;
};

class RepeaterEngine {
public:
    explicit RepeaterEngine(DdDriver& driver);
    ~RepeaterEngine();

    void StartThread();
    void StopThread();

    void UpdateBindings(const std::vector<KeyBinding>& bindings);
    void SetToolRunning(bool running);
    bool IsToolRunning() const noexcept;

    void OnPhysicalDown(const KeyDescriptor& key);
    void OnPhysicalUp(const KeyDescriptor& key);
    void ReleaseAll();

    bool HasActiveHoldKeys() const;

private:
    void WorkerLoop();
    void ResetAllLocked();
    static ULONGLONG NowMs();

    DdDriver& driver_;

    std::atomic<bool> exit_{false};
    std::atomic<bool> running_{false};
    std::thread worker_;

    mutable std::mutex mutex_;

    std::vector<TaskRuntime> tasks_;

    // 物理按键状态追踪（由Hook维护）
    std::unordered_set<KeyDescriptor, KeyDescriptorHash> physicalDown_;
    bool toolRunning_ = false;

    // ⭐ per-key计数器：SendTap前+2，Hook事件到达时-1，计数器>0就忽略
    std::unordered_map<std::uint64_t, int> ignoreCount_;
    void BeforeSend(const KeyDescriptor& key);
    bool ShouldIgnore(const KeyDescriptor& key);

    // ⭐ Hold模式key的独立物理状态追踪（不受ignoreCount影响）
    // 使用独立的KEYUP计数器来过滤SendTap产生的KEYUP
    std::unordered_set<KeyDescriptor, KeyDescriptorHash> holdPhysicalDown_;
    std::unordered_map<std::uint64_t, int> holdUpIgnoreCount_;
    bool IsHoldBinding(const KeyDescriptor& key) const;
    bool IsHoldActive() const;
};
