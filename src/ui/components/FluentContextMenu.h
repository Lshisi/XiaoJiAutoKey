#pragma once

#include <QPoint>

class QLineEdit;
class QPlainTextEdit;

namespace FluentContextMenu {

// 右键编辑菜单（剪切/复制/粘贴/全选）：复用 FluentPopupMenu 的全局风格，
// 替代 Qt 原生右键菜单；只读框仅列出复制/全选，没有可用动作时不弹出。
void popupForLineEdit(QLineEdit* edit, const QPoint& globalPos);
void popupForPlainTextEdit(QPlainTextEdit* edit, const QPoint& globalPos);

}  // namespace FluentContextMenu
