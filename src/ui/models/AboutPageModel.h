#pragma once

#include "PageModel.h"

class AboutPageModel final : public PageModel {
    Q_OBJECT
public:
    explicit AboutPageModel(QObject* parent = nullptr);
    void setUpdateIdle();
    void setChecking();
    void setNoUpdate();
    void setUpdateAvailable(const QString& latestVersion, bool hasAsset);
    void setUpdateBusy();
    void setUpdateStaged();
    Q_INVOKABLE void activateUpdate();

signals:
    void checkUpdateRequested();
    void updateActionRequested();
    void usageGuideRequested();
};
