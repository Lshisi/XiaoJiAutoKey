#include "FluentButton.h"
#include "../styles/DesignTokens.h"
#include "FactionTheme.h"

#include <QPainter>
#include <QPainterPath>
#include <QFontMetricsF>

namespace {
enum class ButtonGlyph { None, Play, Stop, Add };

struct ButtonContent {
    ButtonGlyph glyph = ButtonGlyph::None;
    QString label;
};

ButtonContent parseButtonContent(const QString& text) {
    ButtonContent content;
    content.label = text;
    if (text.startsWith(QString(QChar(0x25B6)))) {
        content.glyph = ButtonGlyph::Play;
        content.label = text.mid(1).trimmed();
    } else if (text.startsWith(QString(QChar(0x25A0)))) {
        content.glyph = ButtonGlyph::Stop;
        content.label = text.mid(1).trimmed();
    } else if (text.startsWith(QStringLiteral("+"))) {
        content.glyph = ButtonGlyph::Add;
        content.label = text.mid(1).trimmed();
    }
    return content;
}

void drawButtonGlyph(QPainter& p, ButtonGlyph glyph, const QRectF& rect, const QColor& color) {
    p.setPen(Qt::NoPen);
    p.setBrush(color);
    const QPointF c = rect.center();
    if (glyph == ButtonGlyph::Play) {
        QPainterPath path;
        path.moveTo(c.x() - 2.6, c.y() - 4.0);
        path.lineTo(c.x() - 2.6, c.y() + 4.0);
        path.lineTo(c.x() + 4.0, c.y());
        path.closeSubpath();
        p.drawPath(path);
    } else if (glyph == ButtonGlyph::Stop) {
        p.drawRoundedRect(QRectF(c.x() - 3.4, c.y() - 3.4, 6.8, 6.8), 1.0, 1.0);
    } else if (glyph == ButtonGlyph::Add) {
        QPen pen(color, 1.7, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);
        p.drawLine(QPointF(c.x() - 4.0, c.y()), QPointF(c.x() + 4.0, c.y()));
        p.drawLine(QPointF(c.x(), c.y() - 4.0), QPointF(c.x(), c.y() + 4.0));
    }
}

void drawButtonContent(QPainter& p, const QRectF& rect, const QString& text, const QFont& font, const QColor& color) {
    const ButtonContent content = parseButtonContent(text);
    p.setFont(font);
    p.setPen(color);

    if (content.glyph == ButtonGlyph::None) {
        p.drawText(rect, Qt::AlignCenter, content.label);
        return;
    }

    const QFontMetricsF metrics(font);
    const qreal iconWidth = 10.0;
    const qreal gap = content.label.isEmpty() ? 0.0 : 6.0;
    const qreal textWidth = content.label.isEmpty() ? 0.0 : metrics.horizontalAdvance(content.label);
    const qreal totalWidth = iconWidth + gap + textWidth;
    const qreal left = rect.center().x() - totalWidth / 2.0;
    const QRectF iconRect(left, rect.top(), iconWidth, rect.height());
    drawButtonGlyph(p, content.glyph, iconRect, color);

    if (!content.label.isEmpty()) {
        p.setPen(color);
        const QRectF textRect(left + iconWidth + gap, rect.top(), textWidth + 1, rect.height());
        p.drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft, content.label);
    }
}
}

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
    p.setRenderHint(QPainter::TextAntialiasing);

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
        drawButtonContent(p, r, text(), font(), textColor);
        return;
    }

    switch (style_) {
    case Primary: {
        const auto palette = FactionTheme::instance().palette();
        QColor base = palette.accent;
        QColor hover = palette.accentHover;
        bgColor = QColor(
            base.red() + (hover.red() - base.red()) * hoverProgress_,
            base.green() + (hover.green() - base.green()) * hoverProgress_,
            base.blue() + (hover.blue() - base.blue()) * hoverProgress_
        );
        textColor = palette.textOnAccent;
        borderColor = Qt::transparent;
        break;
    }
    case Secondary: {
        const auto palette = FactionTheme::instance().palette();
        bgColor = isDown() ? palette.accentSubtle
                           : QColor(255, 255, 255, 255 - (int)(hoverProgress_ * 12));
        if (hoverProgress_ > 0) bgColor = palette.accentSubtle;
        textColor = Fluent::Colors::textPrimary();
        borderColor = hoverProgress_ > 0 ? palette.accentBorder : Fluent::Colors::border();
        break;
    }
    case Danger: {
        bgColor = hoverProgress_ > 0 ? QColor(0xFF, 0xF1, 0xF0) : Qt::white;
        textColor = Fluent::Colors::danger();
        borderColor = Fluent::Colors::border();
        break;
    }
    case Icon: {
        const auto palette = FactionTheme::instance().palette();
        bgColor = hoverProgress_ > 0 ? palette.accentSubtle : Qt::transparent;
        textColor = hoverProgress_ > 0 ? palette.accent : Fluent::Colors::textSecondary();
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

    drawButtonContent(p, r, text(), font(), textColor);
}
