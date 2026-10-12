#pragma once

#include <Windows.h>

#include <optional>

struct ClickPosition;
class InputDriver;

// 提供当前游戏窗口客户区（屏幕坐标），由 ForegroundWatcher 实现。
class GameWindowProvider {
public:
    virtual ~GameWindowProvider() = default;
    virtual bool TryGetGameClientRect(RECT& outRect) const = 0;
};

// 把固定点位换算成屏幕坐标；游戏窗口不可用时返回 nullopt。
[[nodiscard]] std::optional<POINT> ResolveClickAnchorScreenPoint(
    const ClickPosition& position, const GameWindowProvider* gameWindows);

// 用相对移动把光标收敛到目标屏幕坐标。指针加速会让相对位移非线性，
// 因此采用“移动→查询→修正”循环。成功时误差不超过 1 像素。
bool MoveCursorToScreenPoint(InputDriver& driver, int x, int y, int maxIterations = 4);
