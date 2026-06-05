#pragma once

#include <QDialog>
#include <QPoint>

struct CloseConfirmResult {
    bool exit = false;
    bool remember = false;
};

class CloseConfirmDialog : public QDialog {
    Q_OBJECT

public:
    explicit CloseConfirmDialog(QWidget* parent = nullptr);

    [[nodiscard]] CloseConfirmResult decision() const;

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    bool exitSelected_ = false;
    bool rememberSelected_ = false;
    bool dragging_ = false;
    QPoint dragStartGlobal_;
    QPoint dragStartFrame_;
};
