#include "DriverPageModel.h"

DriverPageModel::DriverPageModel(QObject* parent) : PageModel(parent) {
    state_ = {{"mode", 0}, {"summary", tr("正在读取驱动状态")}, {"details", QString()},
        {"xiaojiInstalled", false}, {"ddInstalled", false}, {"hidInstalled", false}, {"busy", false}, {"pluginBusy", false},
        {"installAvailable", false}, {"installText", QString()}, {"providers", QVariantList{}}, {"providerIndex", -1}, {"providerId", QString()}, {"pluginManagement", false},
        {"systemDriverUninstall", false}, {"diagnosticsAvailable", false}, {"selfCheckAvailable", false}};
}

void DriverPageModel::setPluginManagement(bool repair, bool uninstall) {
    if (state_.value("pluginManagement").toBool() == repair && state_.value("systemDriverUninstall").toBool() == uninstall) return;
    state_.insert("pluginManagement", repair);
    state_.insert("systemDriverUninstall", uninstall);
    emit stateChanged();
}

void DriverPageModel::setDiagnosticsActions(bool diagnostics, bool selfCheck) {
    if (state_.value("diagnosticsAvailable").toBool() == diagnostics && state_.value("selfCheckAvailable").toBool() == selfCheck) return;
    state_.insert("diagnosticsAvailable", diagnostics);
    state_.insert("selfCheckAvailable", selfCheck);
    emit stateChanged();
}

void DriverPageModel::managePlugin(const QString& operation) {
    if ((operation == QStringLiteral("repair") && state_.value("pluginManagement").toBool())
        || (operation == QStringLiteral("uninstall") && state_.value("systemDriverUninstall").toBool()))
        emit pluginManagementRequested(operation);
}

void DriverPageModel::setProviders(const QVariantList& providers, const QString& selected) {
    int index = -1;
    for (qsizetype i = 0; i < providers.size(); ++i)
        if (providers[i].toMap().value("id").toString() == selected) index = static_cast<int>(i);
    if (state_.value("providers").toList() == providers && state_.value("providerIndex").toInt() == index
        && state_.value("providerId").toString() == selected) return;
    state_.insert("providers", providers);
    state_.insert("providerIndex", index);
    state_.insert("providerId", selected);
    emit stateChanged();
}

void DriverPageModel::setDriverActions(bool xiaojiInstalled, bool ddInstalled, bool hidInstalled) {
    if (state_.value("xiaojiInstalled").toBool() == xiaojiInstalled && state_.value("ddInstalled").toBool() == ddInstalled
        && state_.value("hidInstalled").toBool() == hidInstalled) return;
    state_.insert("xiaojiInstalled", xiaojiInstalled);
    state_.insert("ddInstalled", ddInstalled);
    state_.insert("hidInstalled", hidInstalled);
    emit stateChanged();
}

void DriverPageModel::setInstallAction(bool available, const QString& text) {
    if (state_.value("installAvailable").toBool() == available && state_.value("installText").toString() == text) return;
    state_.insert("installText", text);
    state_.insert("installAvailable", available);
    emit stateChanged();
}

void DriverPageModel::setDetailText(const QString& value) { setValue("details", value.trimmed()); }

void DriverPageModel::selectMode(int value) {
    if (value < 0 || value > 2) return;
    emit driverModeChanged(static_cast<DriverMode>(value));
}

void DriverPageModel::uninstallMode(int value) {
    if (value < 0 || value > 1) return;
    emit uninstallDriverRequested(static_cast<DriverRemovalTarget>(value));
}
