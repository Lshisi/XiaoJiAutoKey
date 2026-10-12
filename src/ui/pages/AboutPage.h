#pragma once

#include <QWidget>

class QScrollArea;
class QLabel;
class QProgressBar;
class FluentButton;

class AboutPage : public QWidget {
    Q_OBJECT
public:
    explicit AboutPage(QWidget* parent = nullptr);

    void updateThemeColors();
    void refreshIcon();

    // Update card state. The page never touches the network itself; all
    // states are driven by UpdateCoordinator in response to user actions.
    void setUpdateIdle(const QString& message);
    void setChecking();
    void setUpdateAvailable(const QString& latestVersion, const QString& assetUrl);
    void setDownloading();
    void showDownloadProgress(qint64 received, qint64 total);
    void setUpdateStaged();
    void setUpdateError(const QString& message);

protected:
    void showEvent(QShowEvent* event) override;

signals:
    void checkUpdateRequested();
    void downloadUpdateRequested(const QString& assetUrl);
    void restartForUpdateRequested();

private:
    enum class UpdateStatusKind {
        Info,
        Accent,
        Error,
    };

    void setUpdateStatus(const QString& text, UpdateStatusKind kind);
    void applyUpdateCardStyles();

    QScrollArea* scrollArea_ = nullptr;
    QLabel* iconLabel_ = nullptr;
    QLabel* versionValueLabel_ = nullptr;
    FluentButton* checkUpdateBtn_ = nullptr;
    QLabel* updateStatusLabel_ = nullptr;
    FluentButton* downloadUpdateBtn_ = nullptr;
    QProgressBar* downloadProgressBar_ = nullptr;
    QLabel* downloadProgressLabel_ = nullptr;
    FluentButton* restartUpdateBtn_ = nullptr;
    UpdateStatusKind updateStatusKind_ = UpdateStatusKind::Info;
    QString pendingAssetUrl_;
};
