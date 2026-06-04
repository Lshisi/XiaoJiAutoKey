#pragma once

#include "../../include/AppTypes.h"

#include <QWidget>
#include <QLabel>

class QButtonGroup;
class FluentComboBox;
class FluentLineEdit;

class SettingsPage : public QWidget {
    Q_OBJECT
public:
    explicit SettingsPage(QWidget* parent = nullptr);

    void setConfig(const AppConfig& config);
    void setHotkeyText(const QString& text);
    void setConfigPath(const QString& path);
    void setCloseAction(CloseAction action);
    void setFaction(const QString& factionId);
    void setCustomSoundPaths(const QString& onPath, const QString& offPath);

signals:
    void hotkeyEditClicked();
    void beepToggled(bool enabled);
    void closeActionChanged(CloseAction action);
    void openConfigFolder();
    void factionChanged(const QString& factionId);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    QLabel* configPathLabel_;
    QButtonGroup* closeActionGroup_ = nullptr;
    FluentComboBox* factionCombo_ = nullptr;
};
