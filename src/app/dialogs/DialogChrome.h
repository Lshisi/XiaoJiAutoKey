#pragma once

#include <QPoint>

class QDialog;
class QMouseEvent;
class QPaintEvent;
class QPushButton;

namespace DialogChrome {
inline constexpr int bindingEditorWidth = 420;
inline constexpr int bindingEditorHeight = 250;
inline constexpr int macroStepEditorWidth = 420;
inline constexpr int macroStepEditorHeight = 250;

void apply(QDialog* dialog);
QPushButton* createCloseButton(QDialog* dialog);
// 尺寸动态变化后调用：围绕 preferredCenter（改尺寸前的窗口中心）重新摆放，
// 并把窗口钳回“父窗口可见区 ∩ 屏幕”。preferredCenter 无效时退化为保持当前中心。
void clampWithinScreen(QDialog* dialog, const QPoint& preferredCenter = QPoint());
void paint(QDialog* dialog, QPaintEvent* event);
bool mousePress(QDialog* dialog, QMouseEvent* event, bool& dragging, QPoint& dragStartGlobal, QPoint& dragStartFrame);
bool mouseMove(QDialog* dialog, QMouseEvent* event, bool dragging, const QPoint& dragStartGlobal, const QPoint& dragStartFrame);
bool mouseRelease(QMouseEvent* event, bool& dragging);
}
