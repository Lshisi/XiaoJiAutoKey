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

    const QRectF box(0.75, (height() - 16) / 2.0 + 0.25, 15.5, 15.5);
    const auto palette = FactionTheme::instance().palette();
    const bool enabled = isEnabled();

    QColor border = enabled ? QColor(0, 0, 0, 90) : QColor(0, 0, 0, 40);
    QColor fill = QColor(0xFF, 0xFF, 0xFF);

    if (isChecked()) {
        fill = enabled ? palette.accent : QColor(0xC8, 0xC8, 0xC8);
        border = fill;
    }

    QPainterPath boxPath;
    boxPath.addRoundedRect(box, 3.0, 3.0);
    p.fillPath(boxPath, fill);
    p.setPen(QPen(border, 1.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
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
