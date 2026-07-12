#include "FactionMenu.h"

#include "../styles/DesignTokens.h"
#include "FactionIcon.h"
#include "FactionTheme.h"

#include <QEventLoop>
#include <QCursor>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QWheelEvent>
#include <QVector>
#include <QWidget>

namespace {

constexpr int kMenuPadding = 4;
constexpr int kMenuRadius = 8;
constexpr int kItemRadius = 5;
constexpr int kIconSize = 16;
constexpr int kMinMenuWidth = 112;
constexpr int kScrollBarWidth = 4;

class FactionPopup final : public QWidget {
public:
    explicit FactionPopup(const FactionMenu::Options& options, const QPoint& globalPos, QWidget* parent = nullptr)
        : QWidget(parent, Qt::Popup | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint),
          options_(options),
          itemHeight_(qMax(18, options.itemHeight)) {
        setAttribute(Qt::WA_TranslucentBackground, true);
        setAttribute(Qt::WA_NoSystemBackground, true);
        setMouseTracking(true);
        setFont(Fluent::Typography::body());

        const QFontMetrics metrics(font());
        int width = qMax(kMinMenuWidth, options.minWidth);
        for (const auto& faction : FactionTheme::instance().factions()) {
            width = qMax(width, metrics.horizontalAdvance(faction.name) + 62);
        }
        const int itemCount = static_cast<int>(FactionTheme::instance().factions().size());
        const int bottomLimit = popupBottomLimit(globalPos, parent);
        const int availableHeight = qMax(kMenuPadding * 2 + itemHeight_, bottomLimit - globalPos.y());
        if (!options_.drawScrollBar && itemCount > 0) {
            const int fittedItemHeight = (availableHeight - kMenuPadding * 2) / itemCount;
            itemHeight_ = qMin(itemHeight_, qMax(18, fittedItemHeight));
        }
        const int totalHeight = kMenuPadding * 2 + itemCount * itemHeight_;
        setFixedSize(width, qMin(totalHeight, availableHeight));
        ensureCurrentVisible();
    }

    QString selectedId() const { return selectedId_; }
    void setLoop(QEventLoop* loop) { loop_ = loop; }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setRenderHint(QPainter::TextAntialiasing, true);

        const auto palette = FactionTheme::instance().palette();
        const qreal borderWidth = Fluent::Painting::alignedStrokeWidth(1.0, devicePixelRatioF());
        const QRectF panelRect = Fluent::Painting::alignedStrokeRect(rect(), borderWidth);
        QPainterPath panelPath;
        panelPath.addRoundedRect(panelRect, kMenuRadius, kMenuRadius);
        p.fillPath(panelPath, Fluent::Colors::cardBg());
        p.setPen(QPen(Fluent::Colors::border(), borderWidth));
        p.drawPath(panelPath);

        p.setClipPath(panelPath);
        const auto& factions = FactionTheme::instance().factions();
        const QString currentId = FactionTheme::instance().current().id;
        const int totalItems = static_cast<int>(factions.size());
        const int listHeight = height() - kMenuPadding * 2;
        const int maxScroll = maxScrollOffset();
        const int firstIndex = qMax(0, scrollOffset_ / itemHeight_);
        const int lastIndex = qMin(totalItems - 1, (scrollOffset_ + listHeight) / itemHeight_);

        p.save();
        p.setClipRect(QRectF(kMenuPadding, kMenuPadding, width() - kMenuPadding * 2, listHeight));
        for (int i = firstIndex; i <= lastIndex; ++i) {
            const QRectF itemRect(kMenuPadding, kMenuPadding + i * itemHeight_ - scrollOffset_, width() - kMenuPadding * 2, itemHeight_);
            if (i == hoveredIndex_) {
                QPainterPath hoverPath;
                hoverPath.addRoundedRect(itemRect.adjusted(1, 1, -1, -1), kItemRadius, kItemRadius);
                p.fillPath(hoverPath, palette.accentSoft);
            }

            const bool checked = factions[i].id == currentId;
            const QColor textColor = checked ? palette.accent : Fluent::Colors::textPrimary();
            const qreal iconTop = itemRect.center().y() - kIconSize / 2.0;
            const QRectF iconRect(itemRect.left() + 24.0, iconTop, kIconSize, kIconSize);
            if (checked) {
                QPen checkPen(palette.accent, 1.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
                p.setPen(checkPen);
                QPainterPath check;
                const qreal cy = itemRect.center().y();
                check.moveTo(itemRect.left() + 9.0, cy + 0.5);
                check.lineTo(itemRect.left() + 11.5, cy + 3.0);
                check.lineTo(itemRect.left() + 16.5, cy - 3.0);
                p.drawPath(check);
            }

            if (!factions[i].iconPath.isEmpty()) {
                QPixmap icon = FactionIcon::render(factions[i].iconPath, QSize(kIconSize, kIconSize), this,
                    Fluent::Theme::isDarkMode() ? (checked ? palette.accent : Fluent::Colors::textPrimary()) : QColor());
                if (!icon.isNull()) {
                    p.setRenderHint(QPainter::SmoothPixmapTransform, true);
                    p.drawPixmap(iconRect.toRect(), icon);
                }
            }

            p.setFont(font());
            p.setPen(textColor);
            p.drawText(itemRect.adjusted(46, 0, -10, 0), Qt::AlignVCenter | Qt::AlignLeft, factions[i].name);
        }
        p.restore();

        if (maxScroll > 0 && options_.drawScrollBar) {
            const qreal trackHeight = height() - kMenuPadding * 2;
            const qreal handleHeight = qMax<qreal>(28.0, trackHeight * trackHeight / (totalItems * itemHeight_));
            const qreal handleTop = kMenuPadding + (trackHeight - handleHeight) * scrollOffset_ / maxScroll;
            const QRectF handleRect(width() - kMenuPadding - kScrollBarWidth, handleTop, kScrollBarWidth, handleHeight);
            p.setPen(Qt::NoPen);
            p.setBrush(Fluent::Theme::isDarkMode() ? QColor(255, 255, 255, 54) : QColor(0, 0, 0, 42));
            p.drawRoundedRect(handleRect, kScrollBarWidth / 2.0, kScrollBarWidth / 2.0);
        }
    }

    void mouseMoveEvent(QMouseEvent* event) override {
        const int index = itemIndexAt(event->pos());
        if (hoveredIndex_ == index) return;
        hoveredIndex_ = index;
        update();
    }

    void leaveEvent(QEvent*) override {
        if (hoveredIndex_ < 0) return;
        hoveredIndex_ = -1;
        update();
    }

    void mouseReleaseEvent(QMouseEvent* event) override {
        if (event->button() != Qt::LeftButton) return;
        const int index = itemIndexAt(event->pos());
        const auto& factions = FactionTheme::instance().factions();
        if (index >= 0 && index < static_cast<int>(factions.size())) {
            selectedId_ = factions[index].id;
        }
        close();
    }

    void wheelEvent(QWheelEvent* event) override {
        if (maxScrollOffset() <= 0) {
            event->ignore();
            return;
        }

        const int delta = event->angleDelta().y();
        if (delta == 0) {
            event->ignore();
            return;
        }

        const int steps = qMax(1, qAbs(delta) / 120);
        scrollOffset_ = qBound(0, scrollOffset_ - (delta > 0 ? steps : -steps) * itemHeight_, maxScrollOffset());
        hoveredIndex_ = itemIndexAt(mapFromGlobal(QCursor::pos()));
        update();
        event->accept();
    }

    void closeEvent(QCloseEvent* event) override {
        if (loop_) loop_->quit();
        QWidget::closeEvent(event);
    }

private:
    int itemIndexAt(const QPoint& pos) const {
        if (pos.x() < kMenuPadding || pos.x() >= width() - kMenuPadding) return -1;
        const int y = pos.y() - kMenuPadding;
        if (y < 0) return -1;
        const int index = (y + scrollOffset_) / itemHeight_;
        return index < static_cast<int>(FactionTheme::instance().factions().size()) ? index : -1;
    }

    int maxScrollOffset() const {
        const int totalHeight = static_cast<int>(FactionTheme::instance().factions().size()) * itemHeight_;
        const int visibleHeight = height() - kMenuPadding * 2;
        return qMax(0, totalHeight - visibleHeight);
    }

    int popupBottomLimit(const QPoint& globalPos, QWidget* parent) const {
        Q_UNUSED(globalPos);
        if (options_.bottomLimitGlobalY > 0) {
            return options_.bottomLimitGlobalY;
        }
        if (parent && parent->window()) {
            return parent->window()->geometry().bottom() + 1 - options_.bottomMargin;
        }
        return globalPos.y() + kMenuPadding * 2 + static_cast<int>(FactionTheme::instance().factions().size()) * itemHeight_;
    }

    void ensureCurrentVisible() {
        const auto& factions = FactionTheme::instance().factions();
        const QString currentId = FactionTheme::instance().current().id;
        int currentIndex = 0;
        for (int i = 0; i < static_cast<int>(factions.size()); ++i) {
            if (factions[i].id == currentId) {
                currentIndex = i;
                break;
            }
        }

        const int listHeight = height() - kMenuPadding * 2;
        const int itemTop = currentIndex * itemHeight_;
        const int itemBottom = itemTop + itemHeight_;
        if (itemBottom > scrollOffset_ + listHeight) {
            scrollOffset_ = itemBottom - listHeight;
        }
        if (itemTop < scrollOffset_) {
            scrollOffset_ = itemTop;
        }
        scrollOffset_ = qBound(0, scrollOffset_, maxScrollOffset());
    }

    int hoveredIndex_ = -1;
    int scrollOffset_ = 0;
    FactionMenu::Options options_;
    int itemHeight_ = 24;
    QString selectedId_;
    QEventLoop* loop_ = nullptr;
};

}

namespace FactionMenu {

QString exec(QWidget* parent, const QPoint& globalPos, const Options& options) {
    FactionPopup popup(options, globalPos, parent);
    QEventLoop loop;
    popup.setLoop(&loop);
    popup.move(globalPos);
    popup.show();
    popup.raise();
    popup.activateWindow();
    loop.exec();
    return popup.selectedId();
}

}
