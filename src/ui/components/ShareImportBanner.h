#pragma once

#include <QWidget>
#include <QString>

class QLabel;
class QPushButton;
class QTimer;

// 剪贴板检测到分享配置时，在主窗口右下角弹出的独立小提示框（不占页面布局）。
// 点击提示框任意位置发出 importRequested，关闭按钮发出 dismissed，超时自动隐藏。
class ShareImportBanner : public QWidget {
    Q_OBJECT

public:
    explicit ShareImportBanner(QWidget* parent = nullptr);

    [[nodiscard]] QString shareText() const { return shareText_; }

    void showForText(const QString& text);
    void hideBanner();
    void applyTheme();
    // 跟随宿主窗口尺寸把提示框锚回右下角（仅可见时生效）。
    void repositionToHost();

signals:
    void importRequested(const QString& text);
    void dismissed(const QString& text);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

private:
    void reposition();

    QLabel* iconLabel_ = nullptr;
    QLabel* textLabel_ = nullptr;
    QLabel* actionLabel_ = nullptr;
    QPushButton* closeButton_ = nullptr;
    QTimer* autoHideTimer_ = nullptr;
    QString shareText_;
};
