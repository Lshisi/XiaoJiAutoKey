#pragma once

#include "PageModel.h"
#include "AppTypes.h"

#include <QElapsedTimer>
#include <QTimer>

class SettingsPageModel final : public PageModel {
    Q_OBJECT
public:
    explicit SettingsPageModel(QObject* parent = nullptr);
    void setConfig(const AppConfig& config);
    void setHotkeyText(const QString& text) { setValue("hotkey", text); }
    void setStartupAction(StartupAction action) { setValue("startupAction", static_cast<int>(action)); }
    void setCloseAction(CloseAction action) { setValue("closeAction", static_cast<int>(action)); }
    void setFaction(const QString& id);
    void setCrispyThemeState(bool unlocked, bool enabled);
    void setGameGateEnabled(bool value) { setValue("gameGateEnabled", value); }
    void setGameGateProcessCount(int count) { setValue("gameGateProcessCount", count); }
    void setAutoStartEnabled(bool value) { setValue("autoStartEnabled", value); }
    void setAutomaticUpdateChecks(bool value) { setValue("automaticUpdateChecks", value); }
    Q_INVOKABLE void selectStartupAction(int value);
    Q_INVOKABLE void selectCloseAction(int value);
    Q_INVOKABLE void activateFactionEmblem();
    Q_INVOKABLE void showCrispyThemeReceipt();
    Q_INVOKABLE void dismissCrispyThemeReceipt();
    Q_INVOKABLE void selectCrispyTheme();

signals:
    void hotkeyEditClicked();
    void startupActionChanged(StartupAction action);
    void closeActionChanged(CloseAction action);
    void factionChanged(const QString& id);
    void gameGateEnabledChanged(bool enabled);
    void gameGateProcessesEditRequested();
    void autoStartToggled(bool enabled);
    void automaticUpdateChecksToggled(bool enabled);
    void crispyThemeUnlockRequested();
    void crispyThemeSelected();
    void crispyThemeReceiptVisibleChanged(bool visible);

private:
    void refreshCalendar();
    void setCrispyThemeReceiptVisible(bool visible);
    QTimer calendarTimer_;
    QElapsedTimer emblemClickTimer_;
    int emblemClickCount_ = 0;
};
