#pragma once

#include "ClickPositioning.h"

#include <Windows.h>

#include <atomic>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

// 监视前台窗口是否属于游戏客户端（剑网3 旗舰/无界/缘起等），
// 同时向引擎提供游戏窗口客户区，用于固定点位点击锚定。
// 单实例使用：进程内只应 Start 一次。
class ForegroundWatcher final : public GameWindowProvider {
public:
    struct Rules {
        bool enabled = false;
        // 额外识别的进程名（已转小写；支持 "name*" 前缀通配）
        std::vector<std::string> customProcesses;
    };

    // gameForeground 变化时在 watcher 线程回调。
    using ChangeCallback = std::function<void(bool gameForeground)>;

    ForegroundWatcher() = default;
    ~ForegroundWatcher() override;

    ForegroundWatcher(const ForegroundWatcher&) = delete;
    ForegroundWatcher& operator=(const ForegroundWatcher&) = delete;

    bool Start();
    void Stop();

    void SetRules(Rules rules);
    void SetCallback(ChangeCallback callback);

    [[nodiscard]] bool IsGameForeground() const noexcept;

    bool TryGetGameClientRect(RECT& outRect) const override;

    // 内置进程前缀：覆盖 JX3ClientX / JX3ClientX64 / JX3ClientX3DX64(无界 PC)
    // / JX3Client(缘起) 等现役及历史客户端命名。
    [[nodiscard]] static bool MatchesBuiltInProcessName(const std::wstring& lowerExeName);
    [[nodiscard]] static bool MatchesCustomProcess(
        const std::wstring& lowerExeName, const std::vector<std::string>& customProcesses);

private:
    void ThreadMain();
    void EvaluateForeground();
    void SetResult(bool gameForeground, HWND gameWindow);

    std::thread thread_;
    std::atomic<bool> exit_{false};
    std::atomic<bool> gameForeground_{false};
    std::atomic<HWND> gameHwnd_{nullptr};

    std::mutex rulesMutex_;
    Rules rules_;

    std::mutex callbackMutex_;
    ChangeCallback callback_;

    DWORD pendingLeaveTick_ = 0;
};

// 工具函数：取前台窗口进程名（已转小写），失败返回空。
[[nodiscard]] std::wstring QueryForegroundProcessName(HWND foregroundWindow);
