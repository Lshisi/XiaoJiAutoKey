#pragma once

#include "PageModel.h"
#include "AppTypes.h"

class SettingsPageModel final : public PageModel {
    Q_OBJECT
public:
    explicit SettingsPageModel(QObject* parent = nullptr);
    void setConfig(const AppConfig& config);
    void setHotkeyText(const QString& text) { setValue("hotkey", text); }
    void setStartupAction(StartupAction action) { setValue("startupAction", static_cast<int>(action)); }
    void setCloseAction(CloseAction action) { setValue("closeAction", static_cast<int>(action)); }
    void setFaction(const QString& id) { setValue("faction", id); }
    void setGameGateEnabled(bool value) { setValue("gameGateEnabled", value); }
    void setGameGateProcessCount(int count) { setValue("gameGateProcessCount", count); }
    void setAutoStartEnabled(bool value) { setValue("autoStartEnabled", value); }
    Q_INVOKABLE void selectStartupAction(int value);
    Q_INVOKABLE void selectCloseAction(int value);

signals:
    void hotkeyEditClicked();
    void startupActionChanged(StartupAction action);
    void closeActionChanged(CloseAction action);
    void factionChanged(const QString& id);
    void gameGateEnabledChanged(bool enabled);
    void gameGateProcessesEditRequested();
    void autoStartToggled(bool enabled);
};
