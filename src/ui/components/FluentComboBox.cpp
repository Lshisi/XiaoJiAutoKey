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

// 模糊匹配：查询里的字符（忽略空格、忽略大小写）按顺序出现在条目文本中即命中。
bool fuzzyContains(const QString& text, const QString& pattern) {
    if (pattern.isEmpty()) {
        return true;
    }
    int pos = 0;
    for (const QChar& ch : pattern) {
        if (ch.isSpace()) {
            continue;
        }
        const QChar lower = ch.toLower();
        while (pos < text.size() && text.at(pos).toLower() != lower) {
            ++pos;
        }
        if (pos >= text.size()) {
            return false;
        }
        ++pos;
    }
    return true;
}

class FluentComboPopup final : public QWidget {
public:
    explicit FluentComboPopup(FluentComboBox* combo, QWidget* parent = nullptr)
        : QWidget(parent, Qt::Popup | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint),
          combo_(combo) {
        setAttribute(Qt::WA_OpaquePaintEvent, true);
        setAutoFillBackground(true);
        setMouseTracking(true);
    }

    void setItems(const QStringList& items, const QVector<int>& itemRows, int currentIndex, int hoveredIndex) {
        items_ = items;
        itemRows_ = itemRows;
        currentIndex_ = currentIndex;
        hoveredIndex_ = hoveredIndex;
        scrollOffset_ = 0;
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
        // 打开时把当前选中项滚到可视区内（尽量居中）；被过滤掉的项不参与定位。
        const int viewHeight = height() - kPopupPadding * 2;
        const int displayed = itemRows_.indexOf(currentIndex_);
        const int centered = displayed > 0
            ? displayed * kItemHeight - qMax(0, (viewHeight - kItemHeight) / 2)
            : 0;
        scrollOffset_ = qBound(0, centered, qMax(0, itemRows_.size() * kItemHeight - viewHeight));
        update();
    }

    void scrollTo(int offset) {
        const int viewHeight = height() - kPopupPadding * 2;
        const int maxOffset = qMax(0, items_.size() * kItemHeight - viewHeight);
        const int next = qBound(0, offset, maxOffset);
        if (next == scrollOffset_) {
            return;
        }
        scrollOffset_ = next;
        if (underMouse()) {
            setHoverIndex(itemIndexAt(lastMousePos_));
        }
        update();
    }

    void scrollBy(int delta) {
        scrollTo(scrollOffset_ + delta);
    }

    void ensureIndexVisible(int index) {
        const int viewHeight = height() - kPopupPadding * 2;
        const int top = index * kItemHeight;
        if (top < scrollOffset_) {
            scrollTo(top);
        } else if (top + kItemHeight > scrollOffset_ + viewHeight) {
            scrollTo(top + kItemHeight - viewHeight);
        }
    }

    int itemIndexAt(const QPoint& pos) const {
        const int x = pos.x() - kPopupPadding;
        const int y = pos.y() - kPopupPadding;
        if (x < 0 || y < 0 || x >= width() - kPopupPadding * 2 || y >= height() - kPopupPadding * 2) {
            return -1;
        }
        const int index = (y + scrollOffset_) / kItemHeight;
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
        const qreal borderWidth = Fluent::Painting::alignedStrokeWidth(1.0, devicePixelRatioF());
        const QRectF outerRect = Fluent::Painting::alignedStrokeRect(rect(), borderWidth);

        QPainterPath outerPath;
        outerPath.addRoundedRect(outerRect, kPopupRadius, kPopupRadius);

        p.fillPath(outerPath, bg);
        p.setPen(QPen(Fluent::Colors::cardBorder(), borderWidth));
        p.setBrush(Qt::NoBrush);
        p.drawPath(outerPath);

        p.setClipPath(outerPath);
        const QFontMetrics metrics(font());
        const int viewHeight = height() - kPopupPadding * 2;
        const int firstVisible = qMax(0, scrollOffset_ / kItemHeight);
        const int lastVisible = qMin(items_.size() - 1, (scrollOffset_ + viewHeight - 1) / kItemHeight);
        for (int i = firstVisible; i <= lastVisible; ++i) {
            const QRect itemRect(
                kPopupPadding,
                kPopupPadding + i * kItemHeight - scrollOffset_,
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
            const QString text = metrics.elidedText(items_.value(i), Qt::ElideRight, qMax(0, itemRect.width() - leftPad - 20));
            p.drawText(itemRect.adjusted(leftPad, 0, -16, 0), Qt::AlignVCenter | Qt::AlignLeft, text);
        }

        const int contentHeight = items_.size() * kItemHeight;
        if (items_.isEmpty()) {
            p.setFont(font());
            p.setPen(Fluent::Colors::textTertiary());
            p.drawText(QRect(kPopupPadding, kPopupPadding, width() - kPopupPadding * 2, viewHeight),
                Qt::AlignVCenter | Qt::AlignHCenter,
                QString::fromWCharArray(L"\u65e0\u5339\u914d\u9879"));
            return;
        }
        if (contentHeight > viewHeight) {
            const QRectF trackRect(
                width() - kPopupPadding - 4.0,
                kPopupPadding + 2.0,
                3.0,
                viewHeight - 4.0);
            QPainterPath trackPath;
            trackPath.addRoundedRect(trackRect, 1.5, 1.5);
            p.fillPath(trackPath, Fluent::Theme::isDarkMode() ? QColor(255, 255, 255, 26) : QColor(0, 0, 0, 20));

            const qreal thumbHeight = qMax<qreal>(24.0, trackRect.height() * qreal(viewHeight) / qreal(contentHeight));
            const qreal thumbY = trackRect.top()
                + (qreal(scrollOffset_) / qreal(contentHeight - viewHeight)) * (trackRect.height() - thumbHeight);
            QPainterPath thumbPath;
            thumbPath.addRoundedRect(QRectF(trackRect.left(), thumbY, trackRect.width(), thumbHeight), 1.5, 1.5);
            p.fillPath(thumbPath, Fluent::Theme::isDarkMode() ? QColor(255, 255, 255, 90) : QColor(0, 0, 0, 70));
        }
    }

    void mouseMoveEvent(QMouseEvent* event) override {
        lastMousePos_ = event->pos();
        setHoverIndex(itemIndexAt(lastMousePos_));
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
            combo_->acceptPopupIndex(itemRows_.value(index, -1));
        }
    }

    int acceptCandidate() const {
        if (hoveredIndex_ >= 0) {
            return hoveredIndex_;
        }
        const int currentDisplayed = itemRows_.indexOf(currentIndex_);
        if (currentDisplayed >= 0) {
            return currentDisplayed;
        }
        return itemRows_.isEmpty() ? -1 : 0;
    }

    void keyPressEvent(QKeyEvent* event) override {
        if (!combo_) {
            QWidget::keyPressEvent(event);
            return;
        }
        const QString typed = event->text();
        const bool printable = !typed.isEmpty()
            && typed.at(0).isPrint()
            && !(event->modifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier));
        if (combo_->searchable() && printable) {
            combo_->appendSearchText(typed);
            event->accept();
            return;
        }
        if (event->key() == Qt::Key_Escape) {
            if (combo_->searchable() && combo_->hasSearchText()) {
                combo_->clearSearchText();
            } else {
                close();
            }
            event->accept();
            return;
        }
        if (combo_->searchable() && event->key() == Qt::Key_Backspace && combo_->hasSearchText()) {
            combo_->backspaceSearchText();
            event->accept();
            return;
        }
        if (items_.isEmpty()) {
            QWidget::keyPressEvent(event);
            return;
        }
        switch (event->key()) {
        case Qt::Key_Return:
        case Qt::Key_Enter: {
            const int candidate = acceptCandidate();
            if (candidate >= 0) {
                combo_->acceptPopupIndex(itemRows_.value(candidate));
            }
            event->accept();
            return;
        }
        case Qt::Key_Up:
        case Qt::Key_Down: {
            int next = hoveredIndex_ >= 0 ? hoveredIndex_ : qMax(0, itemRows_.indexOf(currentIndex_));
            next = event->key() == Qt::Key_Down
                ? qMin(items_.size() - 1, next + 1)
                : qMax(0, next - 1);
            setHoverIndex(next);
            ensureIndexVisible(next);
            event->accept();
            return;
        }
        case Qt::Key_PageUp:
        case Qt::Key_PageDown:
            scrollBy((event->key() == Qt::Key_PageUp ? -1 : 1) * qMax(kItemHeight, height() - kPopupPadding * 2));
            event->accept();
            return;
        case Qt::Key_Home:
            scrollTo(0);
            event->accept();
            return;
        case Qt::Key_End:
            scrollTo(items_.size() * kItemHeight);
            event->accept();
            return;
        default:
            QWidget::keyPressEvent(event);
            return;
        }
    }

    void wheelEvent(QWheelEvent* event) override {
        const int delta = event->angleDelta().y();
        if (delta == 0) {
            event->ignore();
            return;
        }
        const int notches = qMax(1, qAbs(delta) / 120);
        scrollBy((delta > 0 ? -1 : 1) * notches * 3 * kItemHeight);
        event->accept();
    }

private:
    QPointer<FluentComboBox> combo_;
    QStringList items_;
    QVector<int> itemRows_;
    int currentIndex_ = -1;
    int hoveredIndex_ = -1;
    int scrollOffset_ = 0;
    QPoint lastMousePos_;
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

    if (!popupWidget_) {
        popupWidget_ = new FluentComboPopup(this, window());
        popupWidget_->setObjectName(QStringLiteral("fluentComboPopupRoot"));
        popupWidget_->installEventFilter(this);
    }

    if (auto* popup = static_cast<FluentComboPopup*>(popupWidget_)) {
        popup->setFont(font());
        updatePopupItems();
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
        filterText_.clear();
        update();
        blockSignals(true);
        setCurrentIndex(row);
        blockSignals(false);
        if (popupWidget_) {
            popupWidget_->hide();
        }
        emit currentIndexChanged(row);
    }
}

void FluentComboBox::setSearchable(bool searchable) {
    if (searchable_ == searchable) {
        return;
    }
    searchable_ = searchable;
    if (!searchable_) {
        clearSearchText();
    }
}

void FluentComboBox::appendSearchText(const QString& text) {
    if (!searchable_ || text.isEmpty()) {
        return;
    }
    filterText_ += text;
    updatePopupItems();
    update();
}

void FluentComboBox::backspaceSearchText() {
    if (!searchable_ || filterText_.isEmpty()) {
        return;
    }
    filterText_.chop(1);
    updatePopupItems();
    update();
}

void FluentComboBox::clearSearchText() {
    if (filterText_.isEmpty()) {
        return;
    }
    filterText_.clear();
    updatePopupItems();
    update();
}

void FluentComboBox::updatePopupItems() {
    if (!popupWidget_) {
        return;
    }
    auto* popup = static_cast<FluentComboPopup*>(popupWidget_);

    const QFontMetrics metrics(font());
    int widest = 0;
    const int total = count();
    for (int i = 0; i < total; ++i) {
        widest = qMax(widest, metrics.horizontalAdvance(itemText(i)));
    }
    // 宽度始终按全量条目计算，输入过滤时宽度不跳动。
    const int popupWidth = qMax(width(), widest + 48);

    QStringList items;
    QVector<int> rows;
    items.reserve(total);
    rows.reserve(total);
    for (int i = 0; i < total; ++i) {
        const QString text = itemText(i);
        if (searchable_ && !fuzzyContains(text, filterText_)) {
            continue;
        }
        items.push_back(text);
        rows.push_back(i);
    }

    const int visibleRows = qMax(1, qMin(items.size(), 10));
    popup->setItems(items, rows, currentIndex(), -1);
    popup->setPopupSize(QSize(popupWidth, visibleRows * kItemHeight + kPopupPadding * 2));
}

void FluentComboBox::keyPressEvent(QKeyEvent* event) {
    if (searchable_) {
        const QString typed = event->text();
        const bool printable = !typed.isEmpty()
            && typed.at(0).isPrint()
            && !(event->modifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier));
        if (printable) {
            appendSearchText(typed);
            if (!popupWidget_ || !popupWidget_->isVisible()) {
                showPopup();
            }
            event->accept();
            return;
        }
        if (event->key() == Qt::Key_Backspace && hasSearchText()) {
            backspaceSearchText();
            event->accept();
            return;
        }
        if (event->key() == Qt::Key_Escape && hasSearchText()) {
            clearSearchText();
            event->accept();
            return;
        }
        if (event->key() == Qt::Key_Down || event->key() == Qt::Key_Up) {
            showPopup();
            event->accept();
            return;
        }
    }
    QComboBox::keyPressEvent(event);
}

void FluentComboBox::mousePressEvent(QMouseEvent* event) {
    // 点击框体重新打开时清掉上次的过滤词，从完整列表开始。
    if (searchable_) {
        clearSearchText();
    }
    QComboBox::mousePressEvent(event);
}

bool FluentComboBox::eventFilter(QObject* watched, QEvent* event) {
    if (watched == popupWidget_ && event->type() == QEvent::Hide) {
        // 弹层关闭（点击外部或选中条目）后恢复显示当前项。
        clearSearchText();
    }
    return QComboBox::eventFilter(watched, event);
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

    const qreal logicalBorderWidth = focused ? 2.0 : 1.0;
    const qreal borderWidth = Fluent::Painting::alignedStrokeWidth(logicalBorderWidth, devicePixelRatioF());
    const QRectF r = Fluent::Painting::alignedStrokeRect(rect(), borderWidth);
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

    p.fillPath(path, bg);
    p.setPen(QPen(border, borderWidth));
    p.setBrush(Qt::NoBrush);
    p.drawPath(path);

    const bool searching = searchable_ && !filterText_.isEmpty();
    const QString text = fontMetrics().elidedText(
        searching ? filterText_ + QLatin1Char('|') : currentText(),
        Qt::ElideRight, qMax(0, width() - 44));
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
