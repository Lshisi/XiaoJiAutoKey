#pragma once

#include "AppTypes.h"

#include <QPointer>
#include <QObject>
#include <QString>
#include <QTimer>
#include <functional>

class AboutPageModel;
class KeyPageModel;
class UpdateDialog;
class UpdateService;
class QWidget;

// Wires the about page update card, the update dialog, the key page update
// badge and the update service together. Background checks only update state;
// downloads, installation and restart are always initiated by the user.
class UpdateCoordinator : public QObject {
    Q_OBJECT

public:
    struct Context {
        QWidget* parent = nullptr;
        AboutPageModel* aboutPage = nullptr;
        KeyPageModel* keyPage = nullptr;
        UpdateService* updateService = nullptr;
        AppConfig* config = nullptr;
        std::function<void()> saveConfig;
        std::function<void(const QString& stagingDir)> applyStagedUpdate;
        std::function<void()> navigateToAbout;
    };

    explicit UpdateCoordinator(QObject* parent = nullptr);

    void bind(Context context);

    // Restores the update badge from the last known version stored in the
    // config. Performs no network access.
    void restoreBadgeFromConfig();
    void startBackgroundChecks();
    void refreshBackgroundChecks();

private:
    void handleCheckFinished(bool ok, const QString& message, bool hasUpdate,
        const QString& latestVersion, const QString& assetUrl, const QString& notes);
    // 打开更新弹窗；更新包已就绪时直接进入重启提示状态。
    void openUpdateDialog();
    void requestCheck(bool manual);

    Context context_;
    QPointer<UpdateDialog> updateDialog_;
    QString stagedDir_;
    QString latestKnownVersion_;
    QString pendingAssetUrl_;
    QString pendingNotes_;
    QTimer backgroundTimer_;
    bool checking_ = false;
    bool manualCheck_ = false;
};
