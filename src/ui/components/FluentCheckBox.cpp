#include "FluentCheckBox.h"

#include "../styles/DesignTokens.h"
#include "FactionTheme.h"

#include <QFontMetrics>
#include <QPainter>
#include <QPainterPath>

FluentCheckBox::FluentCheckBox(const QString& text, QWidget* parent)
    : QCheckBox(text, parent) {
    setFont(Fluent::Typography::body());
    setCursor(Qt::PointingHandCursor);
    setAttribute(Qt::WA_TranslucentBackground, true);
}

QSize FluentCheckBox::sizeHint() const {
    const QFontMetrics metrics(font());
    return QSize(24 + metrics.horizontalAdvance(text()), 24);
}

void FluentCheckBox::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const qreal borderWidth = Fluent::Painting::alignedStrokeWidth(1.0, devicePixelRatioF());
    const QRect boxBounds(0, (height() - 16) / 2, 16, 16);
    const QRectF box = Fluent::Painting::alignedStrokeRect(boxBounds, borderWidth);
    const auto palette = FactionTheme::instance().palette();
    const bool enabled = isEnabled();

    QColor border = enabled
        ? (Fluent::Theme::isDarkMode() ? QColor(255, 255, 255, 118) : QColor(0, 0, 0, 90))
        : Fluent::Colors::border();
    QColor fill = Fluent::Colors::cardBg();

    if (isChecked()) {
        fill = enabled ? palette.accent : Fluent::Colors::textTertiary();
        border = fill;
    }

    QPainterPath boxPath;
    boxPath.addRoundedRect(box, 3.0, 3.0);
    p.fillPath(boxPath, fill);
    p.setPen(QPen(border, borderWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.setBrush(Qt::NoBrush);
    p.drawPath(boxPath);

    if (isChecked()) {
        QPainterPath check;
        check.moveTo(box.left() + 4.0, box.center().y() + 0.4);
        check.lineTo(box.left() + 6.8, box.bottom() - 4.3);
        check.lineTo(box.right() - 3.6, box.top() + 4.2);
        p.setPen(QPen(palette.textOnAccent, 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.drawPath(check);
    }

    const QRectF textRect(23, 0, width() - 23, height());
    p.setPen(enabled ? Fluent::Colors::textPrimary() : Fluent::Colors::textTertiary());
    p.setFont(font());
    p.drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft, text());
}
