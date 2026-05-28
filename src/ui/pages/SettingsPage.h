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
    void changeConfigPath();
    void resetConfigPath();
    void factionChanged(const QString& factionId);
    void customSoundOnChanged(const QString& path);
    void customSoundOffChanged(const QString& path);
    void customSoundOnReset();
    void customSoundOffReset();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    static bool isValidAudioFile(const QString& path);

    QLabel* configPathLabel_;
    QButtonGroup* closeActionGroup_ = nullptr;
    FluentComboBox* factionCombo_ = nullptr;
    FluentLineEdit* soundOnPathEdit_ = nullptr;
    FluentLineEdit* soundOffPathEdit_ = nullptr;
};
