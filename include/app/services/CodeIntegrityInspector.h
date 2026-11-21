#pragma once

#include <string>
#include <vector>

// 只读检测 Windows 代码完整性防护（内核隔离）对按键驱动的拦截状态。
// 内存完整性（HVCI）与"易受攻击的驱动程序阻止列表"会拦截 小叽/DD 内核驱动加载，
// 是"连发无效"类售后问题的最常见根因；本类只做检测，绝不修改系统安全设置。
class CodeIntegrityInspector {
public:
    struct BlockedDriverEvent {
        std::wstring fileName;  // 被拦截的驱动文件名（如 dd.key.96688.sys）
        std::wstring timeText;  // 拦截发生时间（本地时间，仅用于展示）
        int eventId = 0;        // CodeIntegrity 事件 ID
    };

    struct GuardStatus {
        bool memoryIntegrityRunning = false;     // 本次开机 HVCI 实际生效（运行时判定）
        bool memoryIntegrityConfigured = false;  // 注册表已配置开启（可能等待重启才生效/失效）
        bool vulnerableBlocklistEnabled = false; // 易受攻击的驱动程序阻止列表（注册表缺省视为开启）
        int smartAppControlState = 0;            // 智能应用控制：0 关 / 1 开 / 2 评估模式
        std::vector<BlockedDriverEvent> recentBlocks; // 近 7 天拦截我方驱动的事件记录
        bool recentBlocksIncomplete = false;

        [[nodiscard]] bool anyDriverGuardActive() const {
            return memoryIntegrityRunning || vulnerableBlocklistEnabled || smartAppControlState == 1;
        }
    };

    // 随包发布的内核驱动文件名，用于事件日志匹配。
    [[nodiscard]] static const std::vector<std::wstring>& bundledKernelDrivers();

    // includeEventLog=false 时跳过事件日志查询（供驱动页等高频刷新场景做轻量检测）。
    [[nodiscard]] static GuardStatus Inspect(bool includeEventLog = false);
};
