#include "AboutPageModel.h"

#include <QTimer>

AboutPageModel::AboutPageModel(QObject* parent) : PageModel(parent) {
    setUpdateIdle();
}

void AboutPageModel::setUpdateIdle() {
    state_ = {{"updateState", QStringLiteral("idle")}, {"latestVersion", QString()},
        {"buttonText", tr("检查更新")}, {"canUpdate", true}};
    emit stateChanged();
}

void AboutPageModel::setChecking() {
    state_.insert("updateState", QStringLiteral("checking"));
    state_.insert("buttonText", tr("检查中…"));
    setValue("canUpdate", false);
}

void AboutPageModel::setNoUpdate() {
    state_.insert("updateState", QStringLiteral("current"));
    state_.insert("buttonText", tr("无更新"));
    setValue("canUpdate", false);
    QTimer::singleShot(3000, this, [this]() {
        if (state_.value("updateState").toString() == QStringLiteral("current")) setUpdateIdle();
    });
}

void AboutPageModel::setUpdateAvailable(const QString& latestVersion, bool hasAsset) {
    state_.insert("updateState", QStringLiteral("available"));
    state_.insert("latestVersion", latestVersion);
    state_.insert("buttonText", hasAsset ? tr("下载并安装") : tr("前往下载"));
    setValue("canUpdate", true);
}

void AboutPageModel::setUpdateBusy() {
    state_.insert("updateState", QStringLiteral("busy"));
    state_.insert("buttonText", tr("正在下载…"));
    setValue("canUpdate", false);
}

void AboutPageModel::setUpdateStaged() {
    state_.insert("updateState", QStringLiteral("staged"));
    state_.insert("buttonText", tr("立即重启更新"));
    setValue("canUpdate", true);
}

void AboutPageModel::activateUpdate() {
    const QString status = state_.value("updateState").toString();
    if (status == QStringLiteral("idle")) emit checkUpdateRequested();
    else if (status == QStringLiteral("available") || status == QStringLiteral("staged")) emit updateActionRequested();
}
