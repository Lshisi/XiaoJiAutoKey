#include "FluentSwitch.h"
#include "../styles/DesignTokens.h"
#include "FactionTheme.h"

#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>

#include <algorithm>

namespace {

QColor blendColor(const QColor& from, const QColor& to, qreal progress) {
    const qreal t = std::clamp(progress, 0.0, 1.0);
    return QColor::fromRgbF(
        from.redF() + (to.redF() - from.redF()) * t,
        from.greenF() + (to.greenF() - from.greenF()) * t,
        from.blueF() + (to.blueF() - from.blueF()) * t,
        from.alphaF() + (to.alphaF() - from.alphaF()) * t);
}

}  // namespace

FluentSwitch::FluentSwitch(QWidget* parent) : QWidget(parent) {
    setFixedSize(44, 22);
    setCursor(Qt::PointingHandCursor);
    anim_ = new QPropertyAnimation(this, "position", this);
    anim_->setDuration(Fluent::Animation::hoverDuration);
    anim_->setEasingCurve(QEasingCurve::OutCubic);
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
    const auto palette = FactionTheme::instance().palette();
    const QColor offTrack = Fluent::Theme::isDarkMode()
        ? QColor(255, 255, 255, hovered_ ? 58 : 42)
        : QColor(0, 0, 0, hovered_ ? 55 : 40);
    const QColor onTrack = hovered_ ? palette.accentHover : palette.accent;
    const QColor trackColor = blendColor(offTrack, onTrack, position_);

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

    const QColor offThumb = Fluent::Theme::isDarkMode() ? QColor(0xD8, 0xD8, 0xD8) : QColor(Qt::white);
    p.setBrush(blendColor(offThumb, QColor(Qt::white), position_));
    p.drawEllipse(QRectF(x, r.top() + margin, thumbDiameter, thumbDiameter));
}
