#pragma once

#include <QDialog>
#include <QPoint>

class ConfigConfirmDialog : public QDialog {
public:
    ConfigConfirmDialog(const QString& title,
        const QString& message,
        const QString& confirmText,
        QWidget* parent = nullptr);

    static bool confirm(QWidget* parent,
        const QString& title,
        const QString& message,
        const QString& confirmText);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    bool dragging_ = false;
    QPoint dragStartGlobal_;
    QPoint dragStartFrame_;
};
