#pragma once

#include "PageModel.h"
#include "AppTypes.h"

class DriverPageModel final : public PageModel {
    Q_OBJECT
public:
    explicit DriverPageModel(QObject* parent = nullptr);
    void setDriverMode(DriverMode mode) { setValue("mode", static_cast<int>(mode)); }
    void setStatusSummary(const QString& value) { setValue("summary", value); }
    void setDetailText(const QString& value);
    void setDriverActions(bool xiaojiInstalled, bool ddInstalled, bool hidInstalled);
    void setInstallAction(bool available, const QString& text);
    void setBusy(bool value) { setValue("busy", value); }
    Q_INVOKABLE void selectMode(int value);
    Q_INVOKABLE void uninstallMode(int value);
    Q_INVOKABLE void uninstallRetiredHid() { emit uninstallDriverRequested(DriverRemovalTarget::RetiredHid); }
    Q_INVOKABLE void diagnosticsRequested() { emit diagnosticsInfoRequested(); }

signals:
    void driverModeChanged(DriverMode mode);
    void refreshRequested();
    void uninstallDriverRequested(DriverRemovalTarget target);
    void selfCheckRequested();
    void diagnosticsInfoRequested();
};
