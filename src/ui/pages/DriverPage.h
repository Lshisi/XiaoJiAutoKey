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
    void setStatusSummary(const QString& summary);
    void setDetailText(const QString& details);
    void setDriverActions(bool ddInstalled, bool hidInstalled);
    void setBusy(bool busy);
    void updateThemeColors();

signals:
    void driverModeChanged(DriverMode mode);
    void refreshRequested();
    void uninstallDriverRequested(DriverMode mode);
    void selfCheckRequested();

private:
    void applyTheme();

    FluentComboBox* driverModeCombo_ = nullptr;
    QLabel* statusSummaryValue_ = nullptr;
    QTextEdit* summaryDetailValue_ = nullptr;
    QWidget* summaryPanel_ = nullptr;
    FluentButton* selfCheckButton_ = nullptr;
    FluentButton* uninstallDdButton_ = nullptr;
    FluentButton* uninstallHidButton_ = nullptr;
};
