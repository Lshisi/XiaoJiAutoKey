#include "dialogs/DialogChrome.h"

#include "ui/components/FluentButton.h"
#include "ui/styles/DesignTokens.h"

#include <QDialog>
#include <QFont>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>

namespace {
constexpr int kRadius = 10;
}

void DialogChrome::apply(QDialog* dialog) {
    if (!dialog) return;
    dialog->setWindowFlags((dialog->windowFlags() | Qt::FramelessWindowHint) & ~Qt::WindowContextHelpButtonHint);
    dialog->setAttribute(Qt::WA_TranslucentBackground, true);
}

QPushButton* DialogChrome::createCloseButton(QDialog* dialog) {
    auto* closeBtn = new FluentButton(FluentButton::Icon, QString(QChar(0x00D7)), dialog);
    QFont closeFont = Fluent::Typography::body();
    closeFont.setPointSize(13);
    closeBtn->setFont(closeFont);
    closeBtn->setFixedSize(28, 28);
    closeBtn->setToolTip(QString::fromWCharArray(L"\u5173\u95ed"));
    closeBtn->setAccessibleName(QString::fromWCharArray(L"\u5173\u95ed"));
    QObject::connect(closeBtn, &QPushButton::clicked, dialog, &QDialog::reject);
    return closeBtn;
}

void DialogChrome::paint(QDialog* dialog, QPaintEvent*) {
    if (!dialog) return;
    QPainter painter(dialog);
    painter.setRenderHint(QPainter::Antialiasing, true);

    QPainterPath path;
    path.addRoundedRect(QRectF(dialog->rect()).adjusted(0.5, 0.5, -0.5, -0.5), kRadius, kRadius);
    painter.fillPath(path, Fluent::Colors::cardBg());
    painter.setPen(QPen(Fluent::Colors::cardBorder(), 1));
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
