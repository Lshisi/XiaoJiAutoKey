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
void paint(QDialog* dialog, QPaintEvent* event);
bool mousePress(QDialog* dialog, QMouseEvent* event, bool& dragging, QPoint& dragStartGlobal, QPoint& dragStartFrame);
bool mouseMove(QDialog* dialog, QMouseEvent* event, bool dragging, const QPoint& dragStartGlobal, const QPoint& dragStartFrame);
bool mouseRelease(QMouseEvent* event, bool& dragging);
}
