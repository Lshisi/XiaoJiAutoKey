#pragma once

#include "AppTypes.h"

#include <QPointer>
#include <QObject>
#include <QString>
#include <functional>

class AboutPageModel;
class KeyPageModel;
class UpdateDialog;
class UpdateService;
class QWidget;

// Wires the about page update card, the update dialog, the key page update
// badge and the update service together. Checks only ever run in response to
// about page actions; nothing here triggers network access on its own.
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

private:
    void handleCheckFinished(bool ok, const QString& message, bool hasUpdate,
        const QString& latestVersion, const QString& assetUrl, const QString& notes);
    // 打开更新弹窗；更新包已就绪时直接进入重启提示状态。
    void openUpdateDialog();

    Context context_;
    QPointer<UpdateDialog> updateDialog_;
    QString stagedDir_;
    QString latestKnownVersion_;
    QString pendingAssetUrl_;
    QString pendingNotes_;
};
