#include "DraggableTable.h"
#include <QApplication>
#include <QHeaderView>
#include <QPainter>

DraggableTable::DraggableTable(int rows, int cols, QWidget* parent)
    : FluentTable(rows, cols, parent) {
    setAcceptDrops(true);
    viewport()->setAcceptDrops(true);
}

void DraggableTable::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        int col = columnAt(event->pos().x());
        int row = rowAt(event->pos().y());
        // 只有点击第一列（拖拽手柄列）才启动拖拽
        if (col == 0 && row >= 0) {
            dragStartRow_ = row;
            dragStartPos_ = event->pos();
        } else {
            dragStartRow_ = -1;
        }
    }
    FluentTable::mousePressEvent(event);
}

void DraggableTable::mouseMoveEvent(QMouseEvent* event) {
    if (dragStartRow_ < 0 || !(event->buttons() & Qt::LeftButton)) {
        FluentTable::mouseMoveEvent(event);
        return;
    }
    if ((event->pos() - dragStartPos_).manhattanLength() < QApplication::startDragDistance()) {
        FluentTable::mouseMoveEvent(event);
        return;
    }

    QDrag* drag = new QDrag(this);
    QMimeData* mimeData = new QMimeData();
    mimeData->setData("application/x-macro-step-row", QByteArray::number(dragStartRow_));
    drag->setMimeData(mimeData);

    // 创建拖拽预览：半透明的行内容
    int rowHeight = this->rowHeight(dragStartRow_);
    int totalWidth = viewport()->width();
    QPixmap pixmap(totalWidth, rowHeight);
    pixmap.fill(QColor(0, 120, 212, 30));
    QPainter painter(&pixmap);
    painter.setPen(QColor(0, 120, 212));
    painter.drawRect(0, 0, totalWidth - 1, rowHeight - 1);
    // 绘制行文本
    for (int c = 1; c < columnCount(); ++c) {
        auto* item = this->item(dragStartRow_, c);
        if (item) {
            int x = columnViewportPosition(c);
            int w = columnWidth(c);
            painter.setPen(Qt::black);
            painter.drawText(QRect(x + 8, 0, w - 16, rowHeight), Qt::AlignLeft | Qt::AlignVCenter, item->text());
        }
    }
    painter.end();
    drag->setPixmap(pixmap);
    drag->setHotSpot(QPoint(event->pos().x(), rowHeight / 2));

    setCursor(Qt::ClosedHandCursor);
    drag->exec(Qt::MoveAction);
    setCursor(Qt::ArrowCursor);
    dragStartRow_ = -1;
    dropIndicatorRow_ = -1;
    viewport()->update();
}

void DraggableTable::dragEnterEvent(QDragEnterEvent* event) {
    if (event->mimeData()->hasFormat("application/x-macro-step-row")) {
        event->acceptProposedAction();
    } else {
        event->ignore();
    }
}

void DraggableTable::dragMoveEvent(QDragMoveEvent* event) {
    if (!event->mimeData()->hasFormat("application/x-macro-step-row")) {
        event->ignore();
        return;
    }
    event->acceptProposedAction();

    // 计算drop指示线位置
    int y = event->pos().y();
    int newIndicator = -1;
    for (int r = 0; r < rowCount(); ++r) {
        int rowTop = rowViewportPosition(r);
        int rowH = rowHeight(r);
        int rowMid = rowTop + rowH / 2;
        if (y < rowMid) {
            newIndicator = r;
            break;
        }
    }
    if (newIndicator < 0) newIndicator = rowCount();

    if (newIndicator != dropIndicatorRow_) {
        dropIndicatorRow_ = newIndicator;
        viewport()->update();
    }
}

void DraggableTable::dropEvent(QDropEvent* event) {
    if (!event->mimeData()->hasFormat("application/x-macro-step-row")) {
        event->ignore();
        return;
    }

    int fromRow = event->mimeData()->data("application/x-macro-step-row").toInt();
    int toRow = dropIndicatorRow_;
    if (toRow < 0) toRow = rowCount();
    // 调整：如果往下移，目标索引需要减1（因为移除源行后索引会变）
    if (toRow > fromRow) toRow--;

    dropIndicatorRow_ = -1;
    viewport()->update();

    if (fromRow != toRow && fromRow >= 0 && fromRow < rowCount()) {
        emit rowMoveRequested(fromRow, toRow);
    }
    event->acceptProposedAction();
}
