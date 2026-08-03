#pragma once

#include "AppTypes.h"

#include <QWidget>

class FluentButton;
class FluentComboBox;
class FluentSwitch;
class QLabel;
class QTextEdit;

class DriverPage : public QWidget {
    Q_OBJECT
public:
    explicit DriverPage(QWidget* parent = nullptr);

    void setDriverMode(DriverMode mode);
    void setStatusSummary(const QString& summary);
    void setDetailText(const QString& details);
    void setDriverActions(bool ddInstalled, bool hidInstalled, bool nativeInstalled,
                          bool usbUpdateAvailable = false);
    void setBusy(bool busy);
    void setBackgroundAutoRun(bool enabled);
    void setBackgroundAutoRunAvailable(bool available);
    void setBackgroundAutoRunBusy(bool busy, bool enabling);
    void setAgentState(const QString& status, bool repairAvailable);
    void updateThemeColors();

signals:
    void driverModeChanged(DriverMode mode);
    void refreshRequested();
    void uninstallDriverRequested(DriverMode mode);
    void updateUsbDriverRequested();
    void repairAgentRequested();
    void backgroundAutoRunToggled(bool enabled);

private:
    void applyTheme();

    FluentComboBox* driverModeCombo_ = nullptr;
    QLabel* statusSummaryValue_ = nullptr;
    QTextEdit* summaryDetailValue_ = nullptr;
    QWidget* summaryPanel_ = nullptr;
    FluentButton* uninstallDdButton_ = nullptr;
    FluentButton* uninstallHidButton_ = nullptr;
    FluentButton* uninstallNativeButton_ = nullptr;
    FluentButton* updateUsbButton_ = nullptr;
    FluentButton* repairAgentButton_ = nullptr;
    QLabel* agentStatusValue_ = nullptr;
    QWidget* backgroundAutoRunSection_ = nullptr;
    QWidget* backgroundAutoRunDivider_ = nullptr;
    FluentSwitch* backgroundAutoRunSwitch_ = nullptr;
    bool busy_ = false;
    bool backgroundAutoRunBusy_ = false;
};
