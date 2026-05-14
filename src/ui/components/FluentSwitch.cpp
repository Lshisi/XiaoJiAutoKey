#include "FluentSwitch.h"
#include "../styles/DesignTokens.h"
#include "../../include/FactionTheme.h"

#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>

FluentSwitch::FluentSwitch(QWidget* parent) : QWidget(parent) {
    setFixedSize(44, 22);
    setCursor(Qt::PointingHandCursor);
    anim_ = new QPropertyAnimation(this, "position", this);
    anim_->setDuration(Fluent::Animation::hoverDuration);
}

void FluentSwitch::setChecked(bool checked) {
    if (checked_ == checked) return;
    checked_ = checked;
    anim_->stop();
    anim_->setStartValue(position_);
    anim_->setEndValue(checked ? 1.0 : 0.0);
    anim_->start();
    emit toggled(checked_);
}

void FluentSwitch::setPosition(qreal pos) {
    position_ = pos;
    update();
}

void FluentSwitch::mousePressEvent(QMouseEvent*) {
    setChecked(!checked_);
}

void FluentSwitch::enterEvent(QEvent*) {
    hovered_ = true;
    update();
}

void FluentSwitch::leaveEvent(QEvent*) {
    hovered_ = false;
    update();
}

void FluentSwitch::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    QRectF r(0.5, 0.5, width() - 1, height() - 1);
    qreal radius = r.height() / 2;

    // Track
    QColor trackColor;
    if (position_ > 0.5) {
        trackColor = FactionTheme::instance().primaryColor();
        if (hovered_) trackColor = FactionTheme::instance().primaryColor().darker(115);
    } else {
        trackColor = QColor(0, 0, 0, 40);
        if (hovered_) trackColor = QColor(0, 0, 0, 55);
    }

    QPainterPath track;
    track.addRoundedRect(r, radius, radius);
    p.setPen(Qt::NoPen);
    p.setBrush(trackColor);
    p.drawPath(track);

    // Thumb
    qreal thumbDiameter = 14;
    qreal margin = (r.height() - thumbDiameter) / 2;
    qreal xStart = r.left() + margin + 1;
    qreal xEnd = r.right() - margin - thumbDiameter - 1;
    qreal x = xStart + (xEnd - xStart) * position_;

    p.setBrush(Qt::white);
    p.drawEllipse(QRectF(x, r.top() + margin, thumbDiameter, thumbDiameter));
}
