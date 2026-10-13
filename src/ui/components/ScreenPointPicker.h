#pragma once

#include <QDialog>
#include <QPoint>

class QLabel;

// 全屏点击式坐标拾取：覆盖整个虚拟桌面，十字光标 + 跟随光标的坐标提示，
// 左键确认、右键 / Esc 取消。
class ScreenPointPicker : public QDialog {
    Q_OBJECT

public:
    explicit ScreenPointPicker(QWidget* parent = nullptr);

    [[nodiscard]] QPoint pickedPos() const noexcept { return pickedPos_; }

protected:
    void showEvent(QShowEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    // 把提示标签挪到光标旁（贴边翻转）并刷新坐标文本。
    void refreshHint();

    QLabel* tip_ = nullptr;
    QPoint pickedPos_;
};
