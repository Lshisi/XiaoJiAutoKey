#include "SettingsPageModel.h"

SettingsPageModel::SettingsPageModel(QObject* parent) : PageModel(parent) {
    state_ = {{"hotkey", tr("未设置")}, {"startupAction", 1}, {"closeAction", 0},
        {"gameGateEnabled", false}, {"autoStartEnabled", false}, {"faction", QStringLiteral("cangjian")}};
}

void SettingsPageModel::setConfig(const AppConfig& config) {
    state_.insert("startupAction", static_cast<int>(config.startupAction));
    state_.insert("closeAction", static_cast<int>(config.closeAction));
    state_.insert("gameGateEnabled", config.gameGateEnabled);
    state_.insert("autoStartEnabled", config.autoStartEnabled);
    setValue("faction", QString::fromStdString(config.faction));
}

void SettingsPageModel::selectStartupAction(int value) {
    if (value < 0 || value > 1) return;
    emit startupActionChanged(static_cast<StartupAction>(value));
}

void SettingsPageModel::selectCloseAction(int value) {
    if (value < 0 || value > 2) return;
    emit closeActionChanged(static_cast<CloseAction>(value));
}
