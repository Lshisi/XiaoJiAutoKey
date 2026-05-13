#include "FluentButton.h"
#include "../styles/DesignTokens.h"

#include <QPainter>
#include <QPainterPath>

FluentButton::FluentButton(const QString& text, QWidget* parent)
    : QPushButton(text, parent)
{
    setupAnimation();
    setButtonStyle(Primary);
}

FluentButton::FluentButton(Style style, const QString& text, QWidget* parent)
    : QPushButton(text, parent)
{
    setupAnimation();
    setButtonStyle(style);
}

void FluentButton::setButtonStyle(Style style) {
    style_ = style;
    setFixedHeight(Fluent::Spacing::buttonHeight);
    setCursor(Qt::PointingHandCursor);
    setFont(Fluent::Typography::body());
    update();
}

void FluentButton::setupAnimation() {
    hoverAnim_ = new QPropertyAnimation(this, "hoverProgress", this);
    hoverAnim_->setDuration(Fluent::Animation::hoverDuration);
}

void FluentButton::setHoverProgress(qreal v) {
    hoverProgress_ = v;
    update();
}

void FluentButton::enterEvent(QEvent*) {
    hoverAnim_->stop();
    hoverAnim_->setStartValue(hoverProgress_);
    hoverAnim_->setEndValue(1.0);
    hoverAnim_->start();
}

void FluentButton::leaveEvent(QEvent*) {
    hoverAnim_->stop();
    hoverAnim_->setStartValue(hoverProgress_);
    hoverAnim_->setEndValue(0.0);
    hoverAnim_->start();
}

void FluentButton::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    QRectF r = rect().adjusted(0, 0, -1, -1);
    QPainterPath path;
    path.addRoundedRect(r, Fluent::Spacing::buttonRadius, Fluent::Spacing::buttonRadius);

    QColor bgColor, textColor, borderColor;

    // Disabled state: gray out
    if (!isEnabled()) {
        bgColor = QColor(0xF0, 0xF0, 0xF0);
        textColor = QColor(0xA0, 0xA0, 0xA0);
        borderColor = QColor(0, 0, 0, 10);
        p.setBrush(bgColor);
        p.setPen(Qt::NoPen);
        p.drawPath(path);
        if (borderColor.alpha() > 0) { p.setPen(QPen(borderColor, 1)); p.setBrush(Qt::NoBrush); p.drawPath(path); }
        p.setPen(textColor);
        p.setFont(font());
        p.drawText(r, Qt::AlignCenter, text());
        return;
    }

    switch (style_) {
    case Primary: {
        QColor base = Fluent::Colors::primary();
        QColor hover = Fluent::Colors::primaryHover();
        bgColor = QColor(
            base.red() + (hover.red() - base.red()) * hoverProgress_,
            base.green() + (hover.green() - base.green()) * hoverProgress_,
            base.blue() + (hover.blue() - base.blue()) * hoverProgress_
        );
        textColor = Qt::white;
        borderColor = Qt::transparent;
        break;
    }
    case Secondary: {
        bgColor = isDown() ? QColor(0xEB, 0xEB, 0xEB)
                           : QColor(255, 255, 255, 255 - (int)(hoverProgress_ * 12));
        if (hoverProgress_ > 0) bgColor = QColor(0xF3, 0xF3, 0xF3);
        textColor = Fluent::Colors::textPrimary();
        borderColor = Fluent::Colors::border();
        break;
    }
    case Danger: {
        bgColor = hoverProgress_ > 0 ? QColor(0xFF, 0xF1, 0xF0) : Qt::white;
        textColor = Fluent::Colors::danger();
        borderColor = Fluent::Colors::border();
        break;
    }
    case Icon: {
        bgColor = hoverProgress_ > 0 ? Fluent::Colors::hoverBg() : Qt::transparent;
        textColor = Fluent::Colors::textSecondary();
        borderColor = Qt::transparent;
        break;
    }
    }

    // Background
    p.setBrush(bgColor);
    p.setPen(Qt::NoPen);
    p.drawPath(path);

    // Border
    if (borderColor.alpha() > 0) {
        p.setPen(QPen(borderColor, 1));
        p.setBrush(Qt::NoBrush);
        p.drawPath(path);
    }

    // Text
    p.setPen(textColor);
    p.setFont(font());
    p.drawText(r, Qt::AlignCenter, text());
}
