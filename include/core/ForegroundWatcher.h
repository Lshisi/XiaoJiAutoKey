#pragma once

#include <Windows.h>

#include <atomic>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

// 监视前台窗口是否属于游戏客户端（剑网3 旗舰/无界/缘起等），
// 供前台门控决定是否挂起引擎。单实例使用：进程内只应 Start 一次。
class ForegroundWatcher final {
public:
    struct Rules {
        bool enabled = false;
        // 额外识别的进程名（已转小写；支持 "name*" 前缀通配）
        std::vector<std::string> customProcesses;
    };

    // gameForeground 变化时在 watcher 线程回调。
    using ChangeCallback = std::function<void(bool gameForeground)>;
    using WindowChangeCallback = std::function<void(HWND foregroundWindow)>;

    ForegroundWatcher() = default;
    ~ForegroundWatcher();

    ForegroundWatcher(const ForegroundWatcher&) = delete;
    ForegroundWatcher& operator=(const ForegroundWatcher&) = delete;

    bool Start();
    void Stop();

    void SetRules(Rules rules);
    void SetCallback(ChangeCallback callback);
    void SetWindowChangeCallback(WindowChangeCallback callback);

    [[nodiscard]] bool IsGameForeground() const noexcept;

    // 内置进程前缀：覆盖 JX3ClientX / JX3ClientX64 / JX3ClientX3DX64(无界 PC)
    // / JX3Client(缘起) 等现役及历史客户端命名。
    [[nodiscard]] static bool MatchesBuiltInProcessName(const std::wstring& lowerExeName);
    [[nodiscard]] static bool MatchesCustomProcess(
        const std::wstring& lowerExeName, const std::vector<std::string>& customProcesses);

private:
    void ThreadMain();
    void EvaluateForeground();
    void SetResult(bool gameForeground);
    void ArmLeaveDebounceTimer();
    void CancelLeaveDebounceTimer();

    static void CALLBACK WinEventProc(HWINEVENTHOOK hook, DWORD event, HWND hwnd,
        LONG idObject, LONG idChild, DWORD eventThread, DWORD eventTime);

    static ForegroundWatcher* activeInstance_;

    std::thread thread_;
    std::atomic<bool> exit_{false};
    std::atomic<bool> gameForeground_{false};

    std::mutex rulesMutex_;
    Rules rules_;

    std::mutex callbackMutex_;
    ChangeCallback callback_;
    WindowChangeCallback windowChangeCallback_;

    HWND lastForegroundWindow_ = nullptr;
    DWORD pendingLeaveTick_ = 0;
    DWORD threadId_ = 0;
    HWINEVENTHOOK winEventHook_ = nullptr;
    UINT_PTR pollTimerId_ = 0;
    UINT_PTR debounceTimerId_ = 0;
};

// 工具函数：取前台窗口进程名（已转小写），失败返回空。
[[nodiscard]] std::wstring QueryForegroundProcessName(HWND foregroundWindow);
