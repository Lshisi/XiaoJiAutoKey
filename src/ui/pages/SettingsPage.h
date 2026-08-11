#pragma once

#include "AppTypes.h"

#include <QWidget>
#include <QLabel>

class QButtonGroup;
class QPushButton;
class QRadioButton;
class FluentLineEdit;
class QSlider;

class SettingsPage : public QWidget {
    Q_OBJECT
public:
    explicit SettingsPage(QWidget* parent = nullptr);

    void setConfig(const AppConfig& config);
    void setHotkeyText(const QString& text);
    void setStartupAction(StartupAction action);
    void setCloseAction(CloseAction action);
    void setFaction(const QString& factionId);
    void setCustomSoundPaths(const QString& onPath, const QString& offPath);

signals:
    void hotkeyEditClicked();
    void beepToggled(bool enabled);
    void statusOverlayOpacityChanged(int opacity);
    void startupActionChanged(StartupAction action);
    void closeActionChanged(CloseAction action);
    void factionChanged(const QString& factionId);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void applyPageTheme();
    void showFactionMenu();
    void updateFactionButtonText();

    QButtonGroup* startupActionGroup_ = nullptr;
    QButtonGroup* closeActionGroup_ = nullptr;
    QPushButton* factionButton_ = nullptr;
    QSlider* statusOverlayOpacitySlider_ = nullptr;
    QLabel* statusOverlayOpacityValue_ = nullptr;
    QLabel* factionPreviewIcon_ = nullptr;
    QLabel* factionPreviewName_ = nullptr;
    QWidget* factionPreviewBar_ = nullptr;
    QRadioButton* radioStartupTray_ = nullptr;
    QRadioButton* radioStartupWindow_ = nullptr;
    QRadioButton* radioAsk_ = nullptr;
    QRadioButton* radioMin_ = nullptr;
    QRadioButton* radioExit_ = nullptr;
};
