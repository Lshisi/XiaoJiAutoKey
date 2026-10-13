#include "ScreenPointPicker.h"

#include "ui/styles/DesignTokens.h"

#include <QCursor>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QRect>
#include <QScreen>

namespace {

// 提示标签与光标的间距。
constexpr int kTipOffset = 18;

QRect ScreenGeometryAt(const QPoint& pos) {
    if (QScreen* screen = QGuiApplication::screenAt(pos)) {
        return screen->geometry();
    }
    if (QScreen* screen = QGuiApplication::primaryScreen()) {
        return screen->geometry();
    }
    return {};
}

}  // namespace

ScreenPointPicker::ScreenPointPicker(QWidget* parent) : QDialog(parent) {
    setWindowFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setModal(true);
    setFocusPolicy(Qt::StrongFocus);
    setCursor(Qt::CrossCursor);
    setMouseTracking(true);

    tip_ = new QLabel(this);
    tip_->setFont(Fluent::Typography::caption());
    tip_->setStyleSheet(QString(
        "QLabel { background: %1; color: %2; border: 1px solid %3;"
        " border-radius: 6px; padding: 6px 10px; }")
        .arg(QColor(32, 32, 32, 235).name(QColor::HexArgb),
            QColor(245, 245, 245).name(QColor::HexArgb),
            QColor(255, 255, 255, 60).name(QColor::HexArgb)));
}

void ScreenPointPicker::showEvent(QShowEvent* event) {
    QDialog::showEvent(event);

    // 覆盖整个虚拟桌面，任意显示器上都能拾取。
    QRect united;
    const auto screens = QGuiApplication::screens();
    for (QScreen* screen : screens) {
        united = united.united(screen->geometry());
    }
    if (!united.isValid() && QGuiApplication::primaryScreen()) {
        united = QGuiApplication::primaryScreen()->geometry();
    }
    if (united.isValid()) {
        setGeometry(united);
    }
    activateWindow();
    setFocus();
    refreshHint();
}

void ScreenPointPicker::paintEvent(QPaintEvent* event) {
    QDialog::paintEvent(event);
    QPainter painter(this);
    // 轻微压暗整屏，提示当前处于拾取模式。
    painter.fillRect(rect(), QColor(0, 0, 0, 38));
}

void ScreenPointPicker::refreshHint() {
    const QPoint pos = QCursor::pos();
    const QRect geometry = ScreenGeometryAt(pos);

    QString coords;
    if (geometry.width() > 0 && geometry.height() > 0) {
        const double rx = static_cast<double>(pos.x() - geometry.left()) / geometry.width();
        const double ry = static_cast<double>(pos.y() - geometry.top()) / geometry.height();
        coords = QString::fromWCharArray(L"\u5c4f\u5e55\u5185 (%1%, %2%)    %3, %4")
            .arg(QString::number(rx * 100.0, 'f', 1),
                QString::number(ry * 100.0, 'f', 1),
                QString::number(pos.x()),
                QString::number(pos.y()));
    }

    tip_->setText(QString::fromWCharArray(
        L"\u5de6\u952e\u9009\u62e9\u4f4d\u7f6e \u00b7 \u53f3\u952e / Esc \u53d6\u6d88\n") + coords);
    tip_->adjustSize();

    QPoint topLeft = pos + QPoint(kTipOffset, kTipOffset);
    if (topLeft.x() + tip_->width() > geometry.right()) {
        topLeft.setX(pos.x() - kTipOffset - tip_->width());
    }
    if (topLeft.y() + tip_->height() > geometry.bottom()) {
        topLeft.setY(pos.y() - kTipOffset - tip_->height());
    }
    tip_->move(topLeft);
}

void ScreenPointPicker::mouseMoveEvent(QMouseEvent* event) {
    refreshHint();
    QDialog::mouseMoveEvent(event);
}

void ScreenPointPicker::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        pickedPos_ = QCursor::pos();
        accept();
        return;
    }
    if (event->button() == Qt::RightButton) {
        reject();
        return;
    }
    QDialog::mousePressEvent(event);
}

void ScreenPointPicker::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Escape) {
        reject();
        return;
    }
    QDialog::keyPressEvent(event);
}
