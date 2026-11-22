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
    void setPluginBusy(bool value) { setValue("pluginBusy", value); }
    void setProviders(const QVariantList& providers, const QString& selected);
    Q_INVOKABLE void selectProvider(const QString& id) { if (!id.isEmpty() && id.size() <= 160) emit providerSelected(id); }
    Q_INVOKABLE void openPluginCenter() { emit pluginCenterRequested(); }
    void setPluginManagement(bool repair, bool uninstall);
    void setDiagnosticsActions(bool diagnostics, bool selfCheck);
    Q_INVOKABLE void managePlugin(const QString& operation);
    Q_INVOKABLE void selectMode(int value);
    Q_INVOKABLE void uninstallMode(int value);
    Q_INVOKABLE void uninstallRetiredHid() { emit uninstallDriverRequested(DriverRemovalTarget::RetiredHid); }
    Q_INVOKABLE void diagnosticsRequested() { if (state_.value("diagnosticsAvailable").toBool()) emit diagnosticsInfoRequested(); }
    Q_INVOKABLE void runSelfCheck() { if (state_.value("selfCheckAvailable").toBool()) emit selfCheckRequested(); }

signals:
    void driverModeChanged(DriverMode mode);
    void refreshRequested();
    void uninstallDriverRequested(DriverRemovalTarget target);
    void selfCheckRequested();
    void diagnosticsInfoRequested();
    void providerSelected(const QString& id);
    void pluginCenterRequested();
    void pluginManagementRequested(const QString& operation);
};
