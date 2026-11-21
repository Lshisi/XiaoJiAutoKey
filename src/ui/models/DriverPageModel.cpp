#include "DriverPageModel.h"

DriverPageModel::DriverPageModel(QObject* parent) : PageModel(parent) {
    state_ = {{"mode", 0}, {"summary", tr("正在读取驱动状态")}, {"details", QString()},
        {"xiaojiInstalled", false}, {"ddInstalled", false}, {"hidInstalled", false}, {"busy", false},
        {"installAvailable", false}, {"installText", QString()}};
}

void DriverPageModel::setDriverActions(bool xiaojiInstalled, bool ddInstalled, bool hidInstalled) {
    state_.insert("xiaojiInstalled", xiaojiInstalled);
    state_.insert("ddInstalled", ddInstalled);
    setValue("hidInstalled", hidInstalled);
}

void DriverPageModel::setInstallAction(bool available, const QString& text) {
    state_.insert("installText", text);
    setValue("installAvailable", available);
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
