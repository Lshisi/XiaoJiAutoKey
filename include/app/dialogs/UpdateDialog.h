#pragma once

#include <QDialog>
#include <QPoint>

class FluentButton;
class QLabel;
class QProgressBar;
class QScrollArea;

// 软件更新弹窗：发现新版本 + 更新日志 → 确认更新 → 下载进度 → 重启提示。
// 弹窗自身不访问网络，所有状态由 UpdateCoordinator 驱动。
class UpdateDialog : public QDialog {
    Q_OBJECT

public:
    UpdateDialog(const QString& version, const QString& notes, bool hasAsset,
        QWidget* parent = nullptr);

    // 确认更新后切换到下载中状态（此期间不允许关闭弹窗）。
    void beginDownload();
    void setDownloadProgress(qint64 received, qint64 total);
    // 下载完成：提示重启。
    void setStagedReady();
    // 下载失败：显示错误并允许重试。
    void setDownloadError(const QString& message);

signals:
    // 确认更新 / 重试。
    void confirmRequested();
    // 立即重启更新。
    void restartRequested();

protected:
    void paintEvent(QPaintEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    void refreshVisuals();

    QString version_;
    bool hasAsset_ = true;
    bool busy_ = false;

    QLabel* headingLabel_ = nullptr;
    QScrollArea* notesScroll_ = nullptr;
    QLabel* notesLabel_ = nullptr;
    QLabel* statusLabel_ = nullptr;
    QProgressBar* progressBar_ = nullptr;
    QLabel* progressLabel_ = nullptr;
    FluentButton* cancelButton_ = nullptr;
    FluentButton* actionButton_ = nullptr;
    QPushButton* closeButton_ = nullptr;

    bool dragging_ = false;
    QPoint dragStartGlobal_;
    QPoint dragStartFrame_;
};
