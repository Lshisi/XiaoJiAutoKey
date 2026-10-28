#include "DriverPageModel.h"

DriverPageModel::DriverPageModel(QObject* parent) : PageModel(parent) {
    state_ = {{"mode", 0}, {"summary", tr("正在读取驱动状态")}, {"details", QString()},
        {"ddInstalled", false}, {"hidInstalled", false}, {"busy", false}};
}

void DriverPageModel::setDriverActions(bool ddInstalled, bool hidInstalled) {
    state_.insert("ddInstalled", ddInstalled);
    setValue("hidInstalled", hidInstalled);
}

void DriverPageModel::selectMode(int value) {
    if (value < 0 || value > 2) return;
    emit driverModeChanged(static_cast<DriverMode>(value));
}

void DriverPageModel::uninstallMode(int value) {
    if (value < 0 || value > 1) return;
    emit uninstallDriverRequested(static_cast<DriverMode>(value));
}
