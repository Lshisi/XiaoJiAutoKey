#pragma once

#include "AppTypes.h"
#include "DdDriver.h"

#include <Windows.h>

#include <atomic>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>

// ── 宏运行时状态 ─────────────────────────────
struct MacroRuntime {
    MacroConfig config;
    bool active = false;            // 当前是否正在执行
    int currentStep = 0;            // 当前执行到第几步
    int loopsCompleted = 0;         // 已完成循环次数
    ULONGLONG lastStepTime = 0;     // 上一步执行的时间戳
    bool waitingDelay = false;      // 是否在等待步骤间隔
    bool keyDown = false;           // 当前步骤是否已按下，等待抬起
    ULONGLONG keyDownTime = 0;      // 按下的时间戳
};

class MacroEngine {
public:
    explicit MacroEngine(DdDriver& driver);
    ~MacroEngine();

    void StartThread();
    void StopThread();

    void UpdateMacros(const std::vector<MacroConfig>& macros);

    // 触发键事件（由Hook调用）
    void OnTriggerDown(const KeyDescriptor& key);
    void OnTriggerUp(const KeyDescriptor& key);

    // 停止所有宏
    void StopAll();

    // 查询状态
    [[nodiscard]] bool HasActiveMacro() const;
    [[nodiscard]] bool IsRunning(std::uint64_t macroId) const;

private:
    void WorkerLoop();
    void ExecuteStep(MacroRuntime& rt);
    static ULONGLONG NowMs();

    DdDriver& driver_;

    std::atomic<bool> exit_{false};
    std::thread worker_;

    mutable std::mutex mutex_;
    std::vector<MacroRuntime> macros_;

    // Hold模式物理按键追踪
    std::unordered_map<std::uint64_t, bool> triggerHeld_;
};
