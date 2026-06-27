#include "FluentComboBox.h"

#include "../styles/DesignTokens.h"
#include "FactionTheme.h"

#include <QApplication>
#include <QEvent>
#include <QFocusEvent>
#include <QFontMetrics>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPointer>
#include <QWheelEvent>
#include <QWidget>

namespace {
constexpr int kPopupPadding = 4;
constexpr int kPopupRadius = 8;
constexpr int kItemHeight = 28;
constexpr int kItemRadius = 4;
constexpr int kArrowWidth = 8;
constexpr int kArrowHeight = 5;

class FluentComboPopup final : public QWidget {
public:
    explicit FluentComboPopup(FluentComboBox* combo, QWidget* parent = nullptr)
        : QWidget(parent, Qt::Popup | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint),
          combo_(combo) {
        setAttribute(Qt::WA_OpaquePaintEvent, true);
        setAutoFillBackground(true);
        setMouseTracking(true);
    }

    void setItems(const QStringList& items, int currentIndex, int hoveredIndex) {
        items_ = items;
        currentIndex_ = currentIndex;
        hoveredIndex_ = hoveredIndex;
        update();
    }

    void setHoverIndex(int index) {
        if (hoveredIndex_ == index) {
            return;
        }
        hoveredIndex_ = index;
        update();
    }

    void setCurrentIndex(int index) {
        currentIndex_ = index;
        update();
    }

    void setPopupSize(const QSize& size) {
        setFixedSize(size);
    }

    int itemIndexAt(const QPoint& pos) const {
        const int x = pos.x() - kPopupPadding;
        const int y = pos.y() - kPopupPadding;
        if (x < 0 || y < 0 || x >= width() - kPopupPadding * 2 || y >= height() - kPopupPadding * 2) {
            return -1;
        }
        const int index = y / kItemHeight;
        return index >= 0 && index < items_.size() ? index : -1;
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setRenderHint(QPainter::TextAntialiasing, true);

        const auto theme = FactionTheme::instance().palette();
        const QColor bg = Fluent::Colors::cardBg();
        p.fillRect(rect(), bg);
        const QRectF outerRect = rect().adjusted(0.5, 0.5, -0.5, -0.5);
        const qreal borderWidth = 1.0;
        const QRectF innerRect = outerRect.adjusted(borderWidth, borderWidth, -borderWidth, -borderWidth);

        QPainterPath outerPath;
        outerPath.addRoundedRect(outerRect, kPopupRadius, kPopupRadius);
        QPainterPath innerPath;
        innerPath.addRoundedRect(innerRect, qMax<qreal>(0.0, kPopupRadius - borderWidth), qMax<qreal>(0.0, kPopupRadius - borderWidth));

        p.fillPath(outerPath, Fluent::Colors::cardBorder());
        p.fillPath(innerPath, bg);

        p.setClipPath(innerPath);
        const QFontMetrics metrics(font());
        for (int i = 0; i < items_.size(); ++i) {
            const QRect itemRect(
                kPopupPadding,
                kPopupPadding + i * kItemHeight,
                width() - kPopupPadding * 2,
                kItemHeight);

            const bool hovered = i == hoveredIndex_;
            const bool current = i == currentIndex_;

            if (hovered || current) {
                QPainterPath itemPath;
                itemPath.addRoundedRect(itemRect.adjusted(2, 2, -2, -2), kItemRadius, kItemRadius);
                p.fillPath(itemPath, hovered ? Fluent::Colors::hoverBg() : theme.accentSoft);
            }

            if (current) {
                QPen checkPen(theme.accent, 1.9, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
                p.setPen(checkPen);
                p.setBrush(Qt::NoBrush);
                const qreal cy = itemRect.center().y();
                QPainterPath check;
                check.moveTo(itemRect.left() + 8.0, cy + 0.5);
                check.lineTo(itemRect.left() + 10.6, cy + 3.0);
                check.lineTo(itemRect.left() + 15.6, cy - 3.0);
                p.drawPath(check);
            }

            const QColor textColor = current ? theme.accent : Fluent::Colors::textPrimary();
            p.setFont(font());
            p.setPen(textColor);
            const int leftPad = current ? 24 : 12;
            const QString text = metrics.elidedText(items_.value(i), Qt::ElideRight, qMax(0, itemRect.width() - leftPad - 14));
            p.drawText(itemRect.adjusted(leftPad, 0, -10, 0), Qt::AlignVCenter | Qt::AlignLeft, text);
        }
    }

    void mouseMoveEvent(QMouseEvent* event) override {
        setHoverIndex(itemIndexAt(event->pos()));
    }

    void leaveEvent(QEvent*) override {
        setHoverIndex(-1);
    }

    void mouseReleaseEvent(QMouseEvent* event) override {
        if (event->button() != Qt::LeftButton) {
            return;
        }
        const int index = itemIndexAt(event->pos());
        if (combo_ && index >= 0) {
            combo_->acceptPopupIndex(index);
        }
    }

    void keyPressEvent(QKeyEvent* event) override {
        if (!combo_) {
            QWidget::keyPressEvent(event);
            return;
        }
        if (event->key() == Qt::Key_Escape) {
            close();
            event->accept();
            return;
        }
        QWidget::keyPressEvent(event);
    }

    void wheelEvent(QWheelEvent* event) override {
        Q_UNUSED(event);
    }

private:
    QPointer<FluentComboBox> combo_;
    QStringList items_;
    int currentIndex_ = -1;
    int hoveredIndex_ = -1;
};
}

FluentComboBox::FluentComboBox(QWidget* parent) : QComboBox(parent) {
    setFocusPolicy(Qt::StrongFocus);
    setCursor(Qt::PointingHandCursor);
    setEditable(false);
    refreshThemeStyle();
}

void FluentComboBox::refreshThemeStyle() {
    const bool hasCustomFixedHeight =
        minimumHeight() == maximumHeight() &&
        minimumHeight() > 0 &&
        minimumHeight() != Fluent::Spacing::inputHeight;
    if (!hasCustomFixedHeight) {
        setFixedHeight(Fluent::Spacing::inputHeight);
    }

    setFont(Fluent::Typography::body());
    setInsertPolicy(QComboBox::NoInsert);
    setSizeAdjustPolicy(QComboBox::AdjustToContentsOnFirstShow);
    setMinimumContentsLength(0);
    setStyleSheet(QString());
    updateGeometry();
    update();
}

void FluentComboBox::showPopup() {
    refreshThemeStyle();

    const int count = model() ? model()->rowCount(rootModelIndex()) : 0;
    if (count <= 0) {
        QComboBox::showPopup();
        return;
    }

    QStringList items;
    items.reserve(count);
    int widest = 0;
    const QFontMetrics metrics(font());
    for (int i = 0; i < count; ++i) {
        const QString text = itemText(i);
        items.push_back(text);
        widest = qMax(widest, metrics.horizontalAdvance(text));
    }

    const int popupWidth = qMax(width(), widest + 48);
    const int visibleRows = qMin(count, 10);
    const int popupHeight = qMin(visibleRows * kItemHeight + kPopupPadding * 2, 10 * kItemHeight + kPopupPadding * 2);
    const QSize popupSize(popupWidth, popupHeight);

    if (!popupWidget_) {
        popupWidget_ = new FluentComboPopup(this, window());
        popupWidget_->setObjectName(QStringLiteral("fluentComboPopupRoot"));
    }

    if (auto* popup = static_cast<FluentComboPopup*>(popupWidget_)) {
        popup->setFont(font());
        popup->setItems(items, currentIndex(), -1);
        popup->setPopupSize(popupSize);
        popup->move(mapToGlobal(QPoint(0, height())));
        popup->show();
        popup->raise();
        popup->activateWindow();
    }
}

void FluentComboBox::hidePopup() {
    if (popupWidget_) {
        popupWidget_->hide();
    }
}

void FluentComboBox::acceptPopupIndex(int row) {
    if (row >= 0 && row < count()) {
        blockSignals(true);
        setCurrentIndex(row);
        blockSignals(false);
        if (popupWidget_) {
            popupWidget_->hide();
        }
        emit currentIndexChanged(row);
    }
}

void FluentComboBox::enterEvent(QEvent* event) {
    QComboBox::enterEvent(event);
    update();
}

void FluentComboBox::leaveEvent(QEvent* event) {
    QComboBox::leaveEvent(event);
    update();
}

void FluentComboBox::focusInEvent(QFocusEvent* event) {
    QComboBox::focusInEvent(event);
    update();
}

void FluentComboBox::focusOutEvent(QFocusEvent* event) {
    QComboBox::focusOutEvent(event);
    update();
}

void FluentComboBox::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    const auto theme = FactionTheme::instance().palette();
    const bool enabled = isEnabled();
    const bool hovered = underMouse();
    const bool focused = hasFocus();

    QRectF r = rect().adjusted(0.5, 0.5, -0.5, -0.5);
    QPainterPath path;
    path.addRoundedRect(r, Fluent::Spacing::inputRadius, Fluent::Spacing::inputRadius);

    QColor bg = Fluent::Colors::cardBg();
    QColor border = Fluent::Colors::border();
    QColor arrowColor = enabled ? Fluent::Colors::textSecondary() : Fluent::Colors::textTertiary();
    if (hovered || focused) {
        border = theme.accentBorder;
    }
    if (!enabled) {
        bg = Fluent::Theme::isDarkMode() ? QColor(255, 255, 255, 16) : QColor(0xF3, 0xF3, 0xF3);
    }

    p.fillRect(rect(), bg);
    const qreal borderWidth = focused ? 1.5 : 1.0;
    const QRectF innerRect = r.adjusted(borderWidth, borderWidth, -borderWidth, -borderWidth);
    QPainterPath innerPath;
    innerPath.addRoundedRect(innerRect, qMax<qreal>(0.0, Fluent::Spacing::inputRadius - borderWidth), qMax<qreal>(0.0, Fluent::Spacing::inputRadius - borderWidth));
    p.fillPath(path, border);
    p.fillPath(innerPath, bg);

    const QString text = fontMetrics().elidedText(currentText(), Qt::ElideRight, qMax(0, width() - 44));
    p.setPen(enabled ? Fluent::Colors::textPrimary() : Fluent::Colors::textTertiary());
    p.setFont(font());
    p.drawText(rect().adjusted(12, 0, -30, 0), Qt::AlignVCenter | Qt::AlignLeft, text);

    const qreal cx = width() - 14.0;
    const qreal cy = height() / 2.0 + 0.5;
    QPen arrowPen(arrowColor, 1.7, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p.setPen(arrowPen);
    p.setBrush(Qt::NoBrush);
    QPainterPath arrow;
    arrow.moveTo(cx - kArrowWidth / 2.0, cy - 0.8);
    arrow.lineTo(cx, cy + kArrowHeight / 2.0);
    arrow.lineTo(cx + kArrowWidth / 2.0, cy - 0.8);
    p.drawPath(arrow);
}
