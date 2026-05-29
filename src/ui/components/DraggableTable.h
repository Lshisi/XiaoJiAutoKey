#pragma once

#include "FluentTable.h"
#include <QMouseEvent>
#include <QDrag>
#include <QMimeData>
#include <QPainter>

class DraggableTable : public FluentTable {
    Q_OBJECT
public:
    explicit DraggableTable(int rows, int cols, QWidget* parent = nullptr);

signals:
    void rowMoveRequested(int fromRow, int toRow);

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private:
    int dragStartRow_ = -1;
    QPoint dragStartPos_;
    int dropIndicatorRow_ = -1;
};
