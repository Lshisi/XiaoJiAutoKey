#include "FluentLineEdit.h"
#include "../styles/DesignTokens.h"
#include "FactionTheme.h"

#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>

FluentLineEdit::FluentLineEdit(QWidget* parent) : QLineEdit(parent) { init(); }
FluentLineEdit::FluentLineEdit(const QString& text, QWidget* parent) : QLineEdit(text, parent) { init(); }

void FluentLineEdit::init() {
    setFixedHeight(Fluent::Spacing::inputHeight);
    setFont(Fluent::Typography::body());
    setFrame(false);
    setTextMargins(12, 0, 12, 0);

    focusAnim_ = new QPropertyAnimation(this, "focusProgress", this);
    focusAnim_->setDuration(Fluent::Animation::hoverDuration);

    // Transparent background so we paint our own
    refreshThemeStyle();
}

void FluentLineEdit::refreshThemeStyle() {
    const bool hasCustomFixedHeight =
        minimumHeight() == maximumHeight() &&
        minimumHeight() > 0 &&
        minimumHeight() != Fluent::Spacing::inputHeight;
    if (!hasCustomFixedHeight) {
        setFixedHeight(Fluent::Spacing::inputHeight);
    }
    setFont(Fluent::Typography::body());
    setTextMargins(12, 0, 12, 0);
    setStyleSheet(QString(
        "QLineEdit { background: transparent; border: none; color: %1; selection-background-color: %2; }"
        "QLineEdit:disabled { color: %3; }")
        .arg(Fluent::Colors::textPrimary().name(),
            FactionTheme::instance().palette().accentSoft.name(),
            Fluent::Colors::textTertiary().name()));
    updateGeometry();
    update();
}

void FluentLineEdit::setFocusProgress(qreal v) {
    focusProgress_ = v;
    update();
}

void FluentLineEdit::focusInEvent(QFocusEvent* event) {
    QLineEdit::focusInEvent(event);
    focusAnim_->stop();
    focusAnim_->setStartValue(focusProgress_);
    focusAnim_->setEndValue(1.0);
    focusAnim_->start();
}

void FluentLineEdit::focusOutEvent(QFocusEvent* event) {
    QLineEdit::focusOutEvent(event);
    focusAnim_->stop();
    focusAnim_->setStartValue(focusProgress_);
    focusAnim_->setEndValue(0.0);
    focusAnim_->start();
}

void FluentLineEdit::mousePressEvent(QMouseEvent* event) {
    QLineEdit::mousePressEvent(event);
    if (event->button() == Qt::LeftButton) emit clicked();
}

void FluentLineEdit::paintEvent(QPaintEvent* event) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    QRectF r = rect().adjusted(0.5, 0.5, -0.5, -0.5);
    QPainterPath path;
    path.addRoundedRect(r, Fluent::Spacing::inputRadius, Fluent::Spacing::inputRadius);

    // Background
    p.setBrush(Fluent::Colors::cardBg());
    p.setPen(Qt::NoPen);
    p.drawPath(path);

    // Border
    QColor borderColor = Fluent::Colors::border();
    if (focusProgress_ > 0) {
        QColor focusColor = FactionTheme::instance().palette().accentBorder;
        borderColor = QColor(
            borderColor.red() + (focusColor.red() - borderColor.red()) * focusProgress_,
            borderColor.green() + (focusColor.green() - borderColor.green()) * focusProgress_,
            borderColor.blue() + (focusColor.blue() - borderColor.blue()) * focusProgress_,
            borderColor.alpha() + (focusColor.alpha() - borderColor.alpha()) * focusProgress_
        );
    }
    p.setPen(QPen(borderColor, focusProgress_ > 0.5 ? 2 : 1));
    p.setBrush(Qt::NoBrush);
    p.drawPath(path);

    // Let QLineEdit draw text
    QLineEdit::paintEvent(event);
}
