#pragma once

#include <QWidget>

class QScrollArea;
class QLabel;
class FluentButton;

class AboutPage : public QWidget {
    Q_OBJECT
public:
    explicit AboutPage(QWidget* parent = nullptr);

    void updateThemeColors();
    void refreshIcon();

    // 更新卡片状态。页面自身从不联网，所有状态由 UpdateCoordinator
    // 在用户操作后驱动；下载 / 重启等细节都在更新弹窗里完成。
    void setUpdateIdle();
    void setChecking();
    void setNoUpdate();
    void setUpdateAvailable(const QString& latestVersion, bool hasAsset);
    void setUpdateBusy();
    void setUpdateStaged();

protected:
    void showEvent(QShowEvent* event) override;

signals:
    // 「检查更新」按钮（空闲态）。
    void checkUpdateRequested();
    // 「下载并安装」/「立即重启更新」按钮：打开更新弹窗。
    void updateActionRequested();

private:
    enum class UpdateButtonState {
        Idle,
        Checking,
        NoUpdate,
        Available,
        Busy,
        Staged,
    };

    void applyUpdateBadgeStyle();

    QScrollArea* scrollArea_ = nullptr;
    QLabel* iconLabel_ = nullptr;
    QLabel* versionValueLabel_ = nullptr;
    QLabel* updateBadgeLabel_ = nullptr;
    FluentButton* checkUpdateBtn_ = nullptr;
    UpdateButtonState buttonState_ = UpdateButtonState::Idle;
    QString latestVersion_;
};
