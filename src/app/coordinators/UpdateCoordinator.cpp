#include "coordinators/UpdateCoordinator.h"

#include "services/UpdateService.h"
#include "ui/pages/AboutPage.h"
#include "ui/pages/KeyPage.h"

#include <QDesktopServices>
#include <QUrl>

UpdateCoordinator::UpdateCoordinator(QObject* parent) : QObject(parent) {}

void UpdateCoordinator::bind(Context context) {
    context_ = std::move(context);
    if (!context_.aboutPage || !context_.keyPage || !context_.updateService) {
        return;
    }

    connect(context_.aboutPage, &AboutPage::checkUpdateRequested, this, [this]() {
        context_.aboutPage->setChecking();
        context_.updateService->checkForUpdate();
    });

    connect(context_.aboutPage, &AboutPage::downloadUpdateRequested, this,
        [this](const QString& assetUrl) {
            if (assetUrl.isEmpty()) {
                // The release has no attached package; fall back to the
                // release page so the user can update manually.
                QDesktopServices::openUrl(QUrl(UpdateService::releasePageUrl()));
                return;
            }
            context_.aboutPage->setDownloading();
            context_.updateService->downloadAndStageUpdate(assetUrl, latestKnownVersion_);
        });

    connect(context_.aboutPage, &AboutPage::restartForUpdateRequested, this, [this]() {
        if (stagedDir_.isEmpty()) {
            return;
        }
        if (context_.applyStagedUpdate) {
            context_.applyStagedUpdate(stagedDir_);
        }
    });

    connect(context_.updateService, &UpdateService::checkFinished, this,
        [this](bool ok, const QString& message, bool hasUpdate,
            const QString& latestVersion, const QString& assetUrl) {
            handleCheckFinished(ok, message, hasUpdate, latestVersion, assetUrl);
        });

    connect(context_.updateService, &UpdateService::downloadProgress, this,
        [this](qint64 received, qint64 total) {
            context_.aboutPage->showDownloadProgress(received, total);
        });

    connect(context_.updateService, &UpdateService::stageFinished, this,
        [this](bool ok, const QString& message, const QString& stagingDir) {
            if (ok) {
                stagedDir_ = stagingDir;
                context_.aboutPage->setUpdateStaged();
            } else {
                stagedDir_.clear();
                context_.aboutPage->setUpdateError(message);
            }
        });

    connect(context_.keyPage, &KeyPage::updateBadgeClicked, this, [this]() {
        if (context_.navigateToAbout) {
            context_.navigateToAbout();
        }
    });
}

void UpdateCoordinator::restoreBadgeFromConfig() {
    latestKnownVersion_.clear();
    if (!context_.config || !context_.keyPage) {
        return;
    }
    const QString stored = QString::fromStdString(context_.config->latestKnownVersion);
    if (!stored.isEmpty() &&
        UpdateService::isVersionNewer(stored, QString::fromLatin1(APP_VERSION))) {
        latestKnownVersion_ = stored;
        context_.keyPage->setUpdateBadge(stored);
    } else {
        context_.keyPage->setUpdateBadge(QString());
    }
}

void UpdateCoordinator::handleCheckFinished(bool ok, const QString& message, bool hasUpdate,
    const QString& latestVersion, const QString& assetUrl) {
    if (!ok) {
        context_.aboutPage->setUpdateError(message);
        return;
    }
    if (!hasUpdate) {
        latestKnownVersion_.clear();
        if (context_.config && !context_.config->latestKnownVersion.empty()) {
            context_.config->latestKnownVersion.clear();
            if (context_.saveConfig) {
                context_.saveConfig();
            }
        }
        if (context_.keyPage) {
            context_.keyPage->setUpdateBadge(QString());
        }
        context_.aboutPage->setNoUpdate();
        return;
    }

    latestKnownVersion_ = latestVersion;
    if (context_.config) {
        context_.config->latestKnownVersion = latestVersion.toStdString();
        if (context_.saveConfig) {
            context_.saveConfig();
        }
    }
    if (context_.keyPage) {
        context_.keyPage->setUpdateBadge(latestVersion);
    }
    context_.aboutPage->setUpdateAvailable(latestVersion, assetUrl);
}
