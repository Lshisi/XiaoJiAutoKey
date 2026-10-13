#pragma once

#include <Windows.h>

#include <optional>

struct ClickPosition;
class InputDriver;

// 把固定点位按“鼠标当前所在显示器”的比例换算成屏幕坐标。
// 点位是全局屏幕坐标：换算不依赖任何窗口；是否只在游戏内生效
// 由设置页的“生效范围”前台门控统一控制，这里不做任何窗口判断。
[[nodiscard]] std::optional<POINT> ResolveClickAnchorScreenPoint(const ClickPosition& position);

// 用相对移动把光标收敛到目标屏幕坐标。指针加速会让相对位移非线性，
// 因此采用“移动→查询→修正”循环并带过冲阻尼；成功时误差不超过 1 像素。
bool MoveCursorToScreenPoint(InputDriver& driver, int x, int y, int maxIterations = 12);
