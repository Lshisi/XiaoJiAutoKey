#pragma once

#include <QPoint>

class QDialog;

namespace DialogChrome {
void apply(QDialog* dialog);
// 尺寸动态变化后调用：围绕 preferredCenter（改尺寸前的窗口中心）重新摆放，
// 并把窗口钳回“父窗口可见区 ∩ 屏幕”。preferredCenter 无效时退化为保持当前中心。
void clampWithinScreen(QDialog* dialog, const QPoint& preferredCenter = QPoint());
}
