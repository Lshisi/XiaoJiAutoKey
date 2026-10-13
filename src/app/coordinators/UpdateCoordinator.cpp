#include "coordinators/UpdateCoordinator.h"

#include "dialogs/AppMessageDialog.h"
#include "dialogs/UpdateDialog.h"
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

    // 「下载并安装」/「立即重启更新」：打开更新弹窗，细节都在弹窗里完成。
    connect(context_.aboutPage, &AboutPage::updateActionRequested, this, [this]() {
        openUpdateDialog();
    });

    connect(context_.updateService, &UpdateService::checkFinished, this,
        [this](bool ok, const QString& message, bool hasUpdate,
            const QString& latestVersion, const QString& assetUrl, const QString& notes) {
            handleCheckFinished(ok, message, hasUpdate, latestVersion, assetUrl, notes);
        });

    connect(context_.updateService, &UpdateService::downloadProgress, this,
        [this](qint64 received, qint64 total) {
            if (updateDialog_) {
                updateDialog_->setDownloadProgress(received, total);
            }
        });

    connect(context_.updateService, &UpdateService::stageFinished, this,
        [this](bool ok, const QString& message, const QString& stagingDir) {
            if (ok) {
                stagedDir_ = stagingDir;
                context_.aboutPage->setUpdateStaged();
                if (updateDialog_) {
                    updateDialog_->setStagedReady();
                }
            } else {
                stagedDir_.clear();
                if (updateDialog_) {
                    updateDialog_->setDownloadError(message);
                } else {
                    AppMessageDialog::warning(context_.aboutPage,
                        QString::fromWCharArray(L"\u66f4\u65b0\u5931\u8d25"), message);
                }
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
    const QString& latestVersion, const QString& assetUrl, const QString& notes) {
    if (!ok) {
        context_.aboutPage->setUpdateIdle();
        AppMessageDialog::warning(context_.aboutPage,
            QString::fromWCharArray(L"\u68c0\u67e5\u66f4\u65b0\u5931\u8d25"), message);
        return;
    }
    if (!hasUpdate) {
        latestKnownVersion_.clear();
        pendingAssetUrl_.clear();
        pendingNotes_.clear();
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
    pendingAssetUrl_ = assetUrl;
    pendingNotes_ = notes;
    if (context_.config) {
        context_.config->latestKnownVersion = latestVersion.toStdString();
        if (context_.saveConfig) {
            context_.saveConfig();
        }
    }
    if (context_.keyPage) {
        context_.keyPage->setUpdateBadge(latestVersion);
    }
    context_.aboutPage->setUpdateAvailable(latestVersion, !assetUrl.isEmpty());
    // 发现新版本：直接弹出更新窗口，展示日志并等待确认。
    openUpdateDialog();
}

void UpdateCoordinator::openUpdateDialog() {
    if (latestKnownVersion_.isEmpty()) {
        return;
    }
    if (updateDialog_) {
        updateDialog_->raise();
        updateDialog_->activateWindow();
        return;
    }

    auto* dialog = new UpdateDialog(latestKnownVersion_, pendingNotes_,
        !pendingAssetUrl_.isEmpty(), context_.aboutPage);
    updateDialog_ = dialog;
    if (!stagedDir_.isEmpty()) {
        dialog->setStagedReady();
    }

    connect(dialog, &UpdateDialog::confirmRequested, this, [this, dialog]() {
        if (pendingAssetUrl_.isEmpty()) {
            // The release has no attached package; fall back to the
            // release page so the user can update manually.
            QDesktopServices::openUrl(QUrl(UpdateService::releasePageUrl()));
            dialog->reject();
            return;
        }
        context_.aboutPage->setUpdateBusy();
        dialog->beginDownload();
        context_.updateService->downloadAndStageUpdate(pendingAssetUrl_, latestKnownVersion_);
    });

    connect(dialog, &UpdateDialog::restartRequested, this, [this]() {
        if (stagedDir_.isEmpty()) {
            return;
        }
        if (context_.applyStagedUpdate) {
            context_.applyStagedUpdate(stagedDir_);
        }
    });

    connect(dialog, &QDialog::finished, this, [this]() {
        // 弹窗关闭后，关于页按钮反映当前进度：已就绪 → 立即重启更新，
        // 否则回到「下载并安装」，点一下就能重新打开弹窗。
        if (stagedDir_.isEmpty()) {
            if (!latestKnownVersion_.isEmpty()) {
                context_.aboutPage->setUpdateAvailable(latestKnownVersion_,
                    !pendingAssetUrl_.isEmpty());
            } else {
                context_.aboutPage->setUpdateIdle();
            }
        } else {
            context_.aboutPage->setUpdateStaged();
        }
    });

    dialog->exec();
    dialog->deleteLater();
    if (updateDialog_ == dialog) {
        updateDialog_ = nullptr;
    }
}
