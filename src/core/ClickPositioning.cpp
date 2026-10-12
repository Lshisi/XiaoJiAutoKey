#include "ClickPositioning.h"

#include "AppTypes.h"
#include "InputDriver.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <thread>

std::optional<POINT> ResolveClickAnchorScreenPoint(
    const ClickPosition& position, const GameWindowProvider* gameWindows) {
    if (!position.IsFixed() || gameWindows == nullptr) {
        return std::nullopt;
    }

    RECT rect{};
    if (!gameWindows->TryGetGameClientRect(rect)) {
        return std::nullopt;
    }

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

    for (int i = 0; i < maxIterations; ++i) {
        POINT current{};
        if (!::GetCursorPos(&current)) {
            return false;
        }
        const int dx = x - static_cast<int>(current.x);
        const int dy = y - static_cast<int>(current.y);
        if (dx == 0 && dy == 0) {
            return true;
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
