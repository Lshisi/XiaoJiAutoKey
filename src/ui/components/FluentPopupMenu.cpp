#include "FluentPopupMenu.h"

#include "../styles/DesignTokens.h"
#include "FactionTheme.h"

#include <QApplication>
#include <QCloseEvent>
#include <QCursor>
#include <QEventLoop>
#include <QFontMetrics>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QScreen>
#include <QWheelEvent>
#include <QWidget>

namespace {

constexpr int kMenuPadding = 4;
constexpr int kMenuRadius = 8;
constexpr int kItemRadius = 5;
constexpr int kScrollBarWidth = 4;
constexpr int kScreenMargin = 4;

class FluentPopup final : public QWidget {
public:
    FluentPopup(
        const QStringList& items,
        const FluentPopupMenu::Options& options,
        QWidget* parent)
        : QWidget(parent, Qt::Popup | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint),
          items_(items),
          options_(options),
          itemHeight_(qMax(20, options.itemHeight)) {
        setAttribute(Qt::WA_TranslucentBackground, true);
        setAttribute(Qt::WA_NoSystemBackground, true);
        setMouseTracking(true);
        setFocusPolicy(Qt::StrongFocus);
        setFont(Fluent::Typography::body());

        const QFontMetrics metrics(font());
        const int trailingWidth = options_.trailingText.isEmpty()
            ? 0
            : metrics.horizontalAdvance(options_.trailingText) + 24;
        int contentWidth = 0;
        for (const auto& item : items_) {
            contentWidth = qMax(contentWidth, metrics.horizontalAdvance(item));
        }
        const int menuWidth = qBound(
            qMax(80, options_.minWidth),
            contentWidth + trailingWidth + 28,
            qMax(options_.minWidth, options_.maxWidth));
        const int visibleItems = qMin(items_.size(), qMax(1, options_.maxVisibleItems));
        setFixedSize(menuWidth, kMenuPadding * 2 + visibleItems * itemHeight_);
    }

    int selectedIndex() const { return selectedIndex_; }
    void setLoop(QEventLoop* loop) { loop_ = loop; }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setRenderHint(QPainter::TextAntialiasing, true);

        const auto factionPalette = FactionTheme::instance().palette();
        const qreal borderWidth = Fluent::Painting::alignedStrokeWidth(1.0, devicePixelRatioF());
        const QRectF panelRect = Fluent::Painting::alignedStrokeRect(rect(), borderWidth);
        QPainterPath panelPath;
        panelPath.addRoundedRect(panelRect, kMenuRadius, kMenuRadius);
        painter.fillPath(panelPath, Fluent::Colors::cardBg());
        painter.setPen(QPen(Fluent::Colors::cardBorder(), borderWidth));
        painter.drawPath(panelPath);

        painter.setClipPath(panelPath);
        const int firstIndex = qMax(0, scrollOffset_ / itemHeight_);
        const int listHeight = height() - kMenuPadding * 2;
        const int lastIndex = qMin(items_.size() - 1, (scrollOffset_ + listHeight) / itemHeight_);
        const QFontMetrics metrics(font());

        painter.save();
        painter.setClipRect(QRect(kMenuPadding, kMenuPadding, width() - kMenuPadding * 2, listHeight));
        for (int i = firstIndex; i <= lastIndex; ++i) {
            const QRectF itemRect(
                kMenuPadding,
                kMenuPadding + i * itemHeight_ - scrollOffset_,
                width() - kMenuPadding * 2,
                itemHeight_);
            const bool rowHovered = options_.trailingText.isEmpty() && i == hoveredIndex_;
            const bool trailingHovered = !options_.trailingText.isEmpty() && i == hoveredIndex_;
            if (rowHovered) {
                QPainterPath hoverPath;
                hoverPath.addRoundedRect(itemRect.adjusted(1, 1, -1, -1), kItemRadius, kItemRadius);
                painter.fillPath(hoverPath, factionPalette.accentSoft);
            }

            const QColor foreground = rowHovered ? factionPalette.accent : Fluent::Colors::textPrimary();
            painter.setFont(font());
            painter.setPen(foreground);
            const int trailingSpace = options_.trailingText.isEmpty() ? 10 : 32;
            const QString text = metrics.elidedText(
                items_[i], Qt::ElideRight, qMax(0, static_cast<int>(itemRect.width()) - trailingSpace - 12));
            painter.drawText(itemRect.adjusted(10, 0, -trailingSpace, 0), Qt::AlignVCenter | Qt::AlignLeft, text);

            if (!options_.trailingText.isEmpty()) {
                const QRectF actionRect = trailingActionRect(itemRect);
                if (trailingHovered) {
                    QPainterPath actionPath;
                    actionPath.addRoundedRect(actionRect, kItemRadius - 1, kItemRadius - 1);
                    painter.fillPath(actionPath, factionPalette.accentSubtle);
                }
                painter.setPen(trailingHovered ? factionPalette.accent : Fluent::Colors::textSecondary());
                painter.drawText(actionRect, Qt::AlignCenter, options_.trailingText);
            }
        }
        painter.restore();

        const int maxScroll = maxScrollOffset();
        if (maxScroll > 0) {
            const qreal trackHeight = height() - kMenuPadding * 2;
            const qreal contentHeight = items_.size() * itemHeight_;
            const qreal handleHeight = qMax<qreal>(24.0, trackHeight * trackHeight / contentHeight);
            const qreal handleTop = kMenuPadding + (trackHeight - handleHeight) * scrollOffset_ / maxScroll;
            const QRectF handleRect(
                width() - kMenuPadding - kScrollBarWidth,
                handleTop,
                kScrollBarWidth,
                handleHeight);
            painter.setPen(Qt::NoPen);
            painter.setBrush(Fluent::Theme::isDarkMode()
                ? QColor(255, 255, 255, 54)
                : QColor(0, 0, 0, 42));
            painter.drawRoundedRect(handleRect, kScrollBarWidth / 2.0, kScrollBarWidth / 2.0);
        }
    }

    void mouseMoveEvent(QMouseEvent* event) override {
        updateHoveredIndex(event->pos());
    }

    void leaveEvent(QEvent*) override {
        if (hoveredIndex_ < 0) return;
        hoveredIndex_ = -1;
        setCursor(Qt::ArrowCursor);
        update();
    }

    void mouseReleaseEvent(QMouseEvent* event) override {
        if (event->button() != Qt::LeftButton) return;
        const int index = itemIndexAt(event->pos());
        if (index < 0 || index >= items_.size()) return;
        if (!options_.trailingText.isEmpty() && !trailingActionRect(itemRectAt(index)).contains(event->pos())) {
            return;
        }
        selectedIndex_ = index;
        close();
    }

    void keyPressEvent(QKeyEvent* event) override {
        if (event->key() == Qt::Key_Escape) {
            close();
            event->accept();
            return;
        }
        QWidget::keyPressEvent(event);
    }

    void wheelEvent(QWheelEvent* event) override {
        const int maxScroll = maxScrollOffset();
        const int delta = event->angleDelta().y();
        if (maxScroll <= 0 || delta == 0) {
            event->ignore();
            return;
        }

        const int steps = qMax(1, qAbs(delta) / 120);
        scrollOffset_ = qBound(
            0,
            scrollOffset_ - (delta > 0 ? steps : -steps) * itemHeight_,
            maxScroll);
        updateHoveredIndex(mapFromGlobal(QCursor::pos()));
        update();
        event->accept();
    }

    void closeEvent(QCloseEvent* event) override {
        if (loop_) loop_->quit();
        QWidget::closeEvent(event);
    }

private:
    QRectF itemRectAt(int index) const {
        return QRectF(
            kMenuPadding,
            kMenuPadding + index * itemHeight_ - scrollOffset_,
            width() - kMenuPadding * 2,
            itemHeight_);
    }

    QRectF trailingActionRect(const QRectF& itemRect) const {
        return itemRect.adjusted(itemRect.width() - 34, 2, -4, -2);
    }

    void updateHoveredIndex(const QPoint& pos) {
        const int index = itemIndexAt(pos);
        const bool actionable = index >= 0 && (options_.trailingText.isEmpty()
            || trailingActionRect(itemRectAt(index)).contains(pos));
        const int nextIndex = actionable ? index : -1;
        setCursor(actionable ? Qt::PointingHandCursor : Qt::ArrowCursor);
        if (nextIndex == hoveredIndex_) return;
        hoveredIndex_ = nextIndex;
        update();
    }

    int itemIndexAt(const QPoint& pos) const {
        if (pos.x() < kMenuPadding || pos.x() >= width() - kMenuPadding) return -1;
        const int y = pos.y() - kMenuPadding;
        if (y < 0 || y >= height() - kMenuPadding * 2) return -1;
        const int index = (y + scrollOffset_) / itemHeight_;
        return index >= 0 && index < items_.size() ? index : -1;
    }

    int maxScrollOffset() const {
        return qMax(0, items_.size() * itemHeight_ - (height() - kMenuPadding * 2));
    }

    QStringList items_;
    FluentPopupMenu::Options options_;
    int itemHeight_ = 28;
    int hoveredIndex_ = -1;
    int selectedIndex_ = -1;
    int scrollOffset_ = 0;
    QEventLoop* loop_ = nullptr;
};

QPoint popupPosition(
    const FluentPopup& popup,
    QWidget* parent,
    const QRect& anchor,
    const FluentPopupMenu::Options& options) {
    QRect bounds;
    if (auto* screen = QGuiApplication::screenAt(anchor.center())) {
        bounds = screen->availableGeometry();
    } else if (parent && parent->window()) {
        bounds = parent->window()->frameGeometry();
    }
    if (!bounds.isValid()) bounds = QRect(anchor.topLeft(), QSize(1920, 1080));

    const int aboveY = anchor.top() - popup.height() - options.anchorGap;
    const int belowY = anchor.bottom() + 1 + options.anchorGap;
    int y = options.preferAbove ? aboveY : belowY;
    if (y < bounds.top() + kScreenMargin || y + popup.height() > bounds.bottom() - kScreenMargin) {
        y = options.preferAbove ? belowY : aboveY;
    }

    const int maxX = bounds.right() - popup.width() - kScreenMargin + 1;
    const int maxY = bounds.bottom() - popup.height() - kScreenMargin + 1;
    const int x = qBound(bounds.left() + kScreenMargin, anchor.right() - popup.width() + 1, maxX);
    y = qBound(bounds.top() + kScreenMargin, y, maxY);
    return QPoint(x, y);
}

}

namespace FluentPopupMenu {

int exec(
    QWidget* parent,
    const QRect& globalAnchorRect,
    const QStringList& items,
    const Options& options) {
    if (items.isEmpty()) return -1;

    FluentPopup popup(items, options, parent);
    QEventLoop loop;
    popup.setLoop(&loop);
    popup.move(popupPosition(popup, parent, globalAnchorRect, options));
    popup.show();
    popup.raise();
    popup.activateWindow();
    popup.setFocus(Qt::PopupFocusReason);
    loop.exec();
    return popup.selectedIndex();
}

}
