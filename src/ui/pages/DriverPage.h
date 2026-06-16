#pragma once

#include "AppTypes.h"

#include <QWidget>

class FluentButton;
class FluentComboBox;
class QLabel;
class QTextEdit;

class DriverPage : public QWidget {
    Q_OBJECT
public:
    explicit DriverPage(QWidget* parent = nullptr);

    void setDriverMode(DriverMode mode);
    void setCurrentRuntime(const QString& runtimeText);
    void setStatusSummary(const QString& summary);
    void setDetailText(const QString& details);
    void setHidUninstallAvailable(bool available);
    void setBusy(bool busy);

signals:
    void driverModeChanged(DriverMode mode);
    void refreshRequested();
    void uninstallHidRequested();

private:
    void applyTheme();

    FluentComboBox* driverModeCombo_ = nullptr;
    QLabel* currentRuntimeValue_ = nullptr;
    QLabel* statusSummaryValue_ = nullptr;
    QTextEdit* summaryDetailValue_ = nullptr;
    QWidget* summaryPanel_ = nullptr;
    FluentButton* uninstallHidButton_ = nullptr;
};
