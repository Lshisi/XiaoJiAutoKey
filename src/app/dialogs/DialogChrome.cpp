#include "dialogs/DialogChrome.h"

#include <QApplication>
#include <QDialog>
#include <QEvent>
#include <QObject>
#include <QQuickItem>
#include <QQuickWidget>
#include <QScreen>

namespace {
QWidget* dialogAnchorWindow(const QDialog* dialog) {
    QWidget* anchor = dialog->parentWidget();
    anchor = anchor ? anchor->window() : QApplication::activeWindow();
    return anchor == dialog ? nullptr : anchor;
}

// 非模态查看窗口及其编辑弹窗以屏幕工作区为边界；其余编辑弹窗限制在父窗口与屏幕的交集。
// 主窗口做过原生窗口 / DPI 几何同步，Qt 侧 frameGeometry 可能大于实际可见区域，
// 单纯按父窗口居中会把弹窗推到窗口底部甚至屏幕外；取交集后异常几何自动退化为屏幕工作区。
QRect dialogBounds(const QDialog* dialog) {
    QWidget* anchor = dialogAnchorWindow(dialog);

    QScreen* screen = dialog->isModal() && anchor && anchor->screen() ? anchor->screen() : dialog->screen();
    if (!screen) screen = QApplication::primaryScreen();
    if (!screen) return {};

    const QRect available = screen->availableGeometry();
    const auto* parentDialog = qobject_cast<QDialog*>(anchor);
    if (!dialog->isModal() || (parentDialog && !parentDialog->isModal())) return available;
    const QRect anchorRect = anchor && anchor->isVisible() ? anchor->frameGeometry() : available;
    const QRect bounds = anchorRect.intersected(available);
    return bounds.isValid() ? bounds : available;
}

QPoint dialogCenter(const QDialog* dialog, const QRect& bounds) {
    QWidget* window = dialogAnchorWindow(dialog);
    auto* view = window ? window->findChild<QQuickWidget*>(QStringLiteral("mainQuickView")) : nullptr;
    auto* anchor = view && view->isVisible() && view->rootObject()
        ? view->rootObject()->findChild<QQuickItem*>(QStringLiteral("workspaceDialogAnchor")) : nullptr;
    if (!anchor || !anchor->isVisible() || anchor->width() <= 0 || anchor->height() <= 0) {
        if (window && window->isVisible()) return window->frameGeometry().intersected(bounds).center();
        return bounds.center();
    }
    return view->mapToGlobal(anchor->mapToScene(
        QPointF(anchor->width() / 2, anchor->height() / 2)).toPoint());
}

QPoint clampedTopLeft(const QRect& bounds, const QPoint& topLeft, const QSize& size) {
    QPoint result = topLeft;
    result.setX(qBound(bounds.left(), result.x(),
        qMax(bounds.left(), bounds.right() - size.width() + 1)));
    result.setY(qBound(bounds.top(), result.y(),
        qMax(bounds.top(), bounds.bottom() - size.height() + 1)));
    return result;
}

// QRect::center() 定义为 (left+right)/2（整除），反推左上角必须用 (size-1)/2 才能精确还原；
// 若用 size/2，偶数宽高的弹窗每调用一次就会向左/上漂移 1px，录制时按键事件连发会导致窗口持续左移。
QPoint topLeftForCenter(const QPoint& center, const QSize& size) {
    return QPoint(center.x() - (size.width() - 1) / 2,
        center.y() - (size.height() - 1) / 2);
}

void centerDialogOnParent(QDialog* dialog) {
    if (!dialog) return;

    const QRect bounds = dialogBounds(dialog);
    if (!bounds.isValid()) return;

    dialog->move(clampedTopLeft(bounds, topLeftForCenter(dialogCenter(dialog, bounds), dialog->size()), dialog->size()));
}

class DialogPositioner final : public QObject {
public:
    explicit DialogPositioner(QDialog* dialog)
        : QObject(dialog), dialog_(dialog) {}

protected:
    bool eventFilter(QObject* watched, QEvent* event) override {
        if (!centered_ && watched == dialog_ && event->type() == QEvent::Show) {
            centered_ = true;
            centerDialogOnParent(dialog_);
        }
        return QObject::eventFilter(watched, event);
    }

private:
    QDialog* dialog_ = nullptr;
    bool centered_ = false;
};
}

void DialogChrome::clampWithinScreen(QDialog* dialog, const QPoint& preferredCenter) {
    if (!dialog || !dialog->isVisible()) return;

    const QRect bounds = dialogBounds(dialog);
    if (!bounds.isValid()) return;

    // 尺寸动态变化（如录了鼠标键长高）：围绕改尺寸前的中心对称生长并钳回可见区域，
    // 避免只向下生长把底部顶到窗口边缘。注意必须在 setFixedSize 之前取中心传入，
    // 否则 frameGeometry 已经是“旧左上角 + 新尺寸”，等价于钉住左上角往下长。
    const QPoint center = preferredCenter.isNull()
        ? dialog->frameGeometry().center()
        : preferredCenter;
    dialog->move(clampedTopLeft(bounds, topLeftForCenter(center, dialog->size()), dialog->size()));
}

void DialogChrome::apply(QDialog* dialog) {
    if (!dialog) return;
    dialog->setWindowFlags((dialog->windowFlags() | Qt::FramelessWindowHint) & ~Qt::WindowContextHelpButtonHint);
    dialog->setAttribute(Qt::WA_TranslucentBackground, true);
    dialog->installEventFilter(new DialogPositioner(dialog));
}
