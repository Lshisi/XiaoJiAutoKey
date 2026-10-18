#include "dialogs/DialogChrome.h"

#include "ui/components/FluentButton.h"
#include "ui/styles/DesignTokens.h"

#include <QApplication>
#include <QDialog>
#include <QEvent>
#include <QFont>
#include <QMouseEvent>
#include <QObject>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QScreen>

namespace {
constexpr int kRadius = 10;

// 弹窗的可见边界：父窗口矩形 ∩ 所在屏幕工作区。
// 主窗口做过原生窗口 / DPI 几何同步，Qt 侧 frameGeometry 可能大于实际可见区域，
// 单纯按父窗口居中会把弹窗推到窗口底部甚至屏幕外；取交集后异常几何自动退化为屏幕工作区。
QRect dialogBounds(const QDialog* dialog) {
    QWidget* anchor = dialog->parentWidget();
    anchor = anchor ? anchor->window() : QApplication::activeWindow();
    if (anchor == const_cast<QDialog*>(dialog)) anchor = nullptr;

    QScreen* screen = anchor && anchor->screen() ? anchor->screen() : dialog->screen();
    if (!screen) screen = QApplication::primaryScreen();
    if (!screen) return {};

    const QRect available = screen->availableGeometry();
    const QRect anchorRect = anchor && anchor->isVisible() ? anchor->frameGeometry() : available;
    const QRect bounds = anchorRect.intersected(available);
    return bounds.isValid() ? bounds : available;
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

    dialog->move(clampedTopLeft(bounds, topLeftForCenter(bounds.center(), dialog->size()), dialog->size()));
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

QPushButton* DialogChrome::createCloseButton(QDialog* dialog) {
    auto* closeBtn = new FluentButton(FluentButton::Icon, QString(QChar(0x00D7)), dialog);
    QFont closeFont = Fluent::Typography::body();
    closeFont.setPointSize(13);
    closeBtn->setFont(closeFont);
    closeBtn->setFixedSize(28, 28);
    closeBtn->setAccessibleName(QString::fromWCharArray(L"\u5173\u95ed"));
    QObject::connect(closeBtn, &QPushButton::clicked, dialog, &QDialog::reject);
    return closeBtn;
}

void DialogChrome::paint(QDialog* dialog, QPaintEvent*) {
    if (!dialog) return;
    QPainter painter(dialog);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const qreal borderWidth = Fluent::Painting::alignedStrokeWidth(1.0, dialog->devicePixelRatioF());
    QPainterPath path;
    path.addRoundedRect(Fluent::Painting::alignedStrokeRect(dialog->rect(), borderWidth), kRadius, kRadius);
    painter.fillPath(path, Fluent::Colors::cardBg());
    painter.setPen(QPen(Fluent::Colors::cardBorder(), borderWidth));
    painter.drawPath(path);
}

bool DialogChrome::mousePress(QDialog* dialog,
    QMouseEvent* event,
    bool& dragging,
    QPoint& dragStartGlobal,
    QPoint& dragStartFrame) {
    if (!dialog || !event || event->button() != Qt::LeftButton) return false;
    dragging = true;
    dragStartGlobal = event->globalPos();
    dragStartFrame = dialog->frameGeometry().topLeft();
    event->accept();
    return true;
}

bool DialogChrome::mouseMove(QDialog* dialog,
    QMouseEvent* event,
    bool dragging,
    const QPoint& dragStartGlobal,
    const QPoint& dragStartFrame) {
    if (!dialog || !event || !dragging || !(event->buttons() & Qt::LeftButton)) return false;
    dialog->move(dragStartFrame + event->globalPos() - dragStartGlobal);
    event->accept();
    return true;
}

bool DialogChrome::mouseRelease(QMouseEvent* event, bool& dragging) {
    if (!event || event->button() != Qt::LeftButton || !dragging) return false;
    dragging = false;
    event->accept();
    return true;
}
