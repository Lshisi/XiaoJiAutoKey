#pragma once

#include "AppTypes.h"

#include <QWidget>
#include <QLabel>

class QButtonGroup;
class QRadioButton;
class FluentComboBox;
class FluentLineEdit;
class QSlider;

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
    void statusOverlayOpacityChanged(int opacity);
    void closeActionChanged(CloseAction action);
    void openConfigFolder();
    void factionChanged(const QString& factionId);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void applyPageTheme();

    QLabel* configPathLabel_;
    QButtonGroup* closeActionGroup_ = nullptr;
    FluentComboBox* factionCombo_ = nullptr;
    QSlider* statusOverlayOpacitySlider_ = nullptr;
    QLabel* statusOverlayOpacityValue_ = nullptr;
    QLabel* factionPreviewIcon_ = nullptr;
    QLabel* factionPreviewName_ = nullptr;
    QWidget* factionPreviewBar_ = nullptr;
    QRadioButton* radioAsk_ = nullptr;
    QRadioButton* radioMin_ = nullptr;
    QRadioButton* radioExit_ = nullptr;
};
