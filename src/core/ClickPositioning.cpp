#include "ClickPositioning.h"

#include "AppTypes.h"
#include "InputDriver.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <thread>

std::optional<POINT> ResolveClickAnchorScreenPoint(const ClickPosition& position) {
    if (!position.IsFixed()) {
        return std::nullopt;
    }

    POINT cursor{};
    if (!::GetCursorPos(&cursor)) {
        return std::nullopt;
    }
    const HMONITOR monitor = ::MonitorFromPoint(cursor, MONITOR_DEFAULTTONEAREST);
    MONITORINFO monitorInfo{};
    monitorInfo.cbSize = sizeof(monitorInfo);
    if (monitor == nullptr || !::GetMonitorInfoW(monitor, &monitorInfo)) {
        return std::nullopt;
    }

    const RECT& rect = monitorInfo.rcMonitor;
    const LONG width = rect.right - rect.left;
    const LONG height = rect.bottom - rect.top;
    if (width <= 0 || height <= 0) {
        return std::nullopt;
    }

    const double x = std::clamp(position.anchorX, 0.0, 1.0);
    const double y = std::clamp(position.anchorY, 0.0, 1.0);
    return POINT{
        rect.left + static_cast<LONG>(std::lround(width * x)),
        rect.top + static_cast<LONG>(std::lround(height * y)),
    };
}

bool MoveCursorToScreenPoint(InputDriver& driver, int x, int y, int maxIterations) {
    if (!driver.SupportsMouseMove()) {
        return false;
    }

    // 指针加速（“提高指针精确度”）让相对位移非线性：残差被放大后会过冲震荡。
    // 检测到残差不缩小时逐级减半步长做阻尼，保证收敛。
    int damping = 0;
    int lastResidual = std::numeric_limits<int>::max();
    for (int i = 0; i < maxIterations; ++i) {
        POINT current{};
        if (!::GetCursorPos(&current)) {
            return false;
        }
        int dx = x - static_cast<int>(current.x);
        int dy = y - static_cast<int>(current.y);
        const int residual = std::abs(dx) + std::abs(dy);
        if (residual == 0) {
            return true;
        }
        if (residual >= lastResidual && damping < 6) {
            ++damping;
        }
        lastResidual = residual;

        if (damping > 0) {
            const auto damped = [&damping](int delta) {
                int step = delta / (1 << damping);
                if (step == 0) {
                    step = delta > 0 ? 1 : (delta < 0 ? -1 : 0);
                }
                return step;
            };
            dx = damped(dx);
            dy = damped(dy);
        }
        if (!driver.SendMouseMove(dx, dy)) {
            return false;
        }
        // 等光标位置更新后再复查，避免读到旧坐标。
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    POINT current{};
    if (!::GetCursorPos(&current)) {
        return false;
    }
    return std::abs(x - static_cast<int>(current.x)) <= 1
        && std::abs(y - static_cast<int>(current.y)) <= 1;
}
