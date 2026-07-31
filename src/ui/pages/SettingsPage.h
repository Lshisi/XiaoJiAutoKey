#pragma once

#include "AppTypes.h"

#include <QWidget>
#include <QLabel>

class QButtonGroup;
class QPushButton;
class QRadioButton;
class FluentLineEdit;
class FluentSwitch;
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
    void backgroundAutoRunToggled(bool enabled);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void applyPageTheme();
    void showFactionMenu();
    void updateFactionButtonText();

    QLabel* configPathLabel_;
    QButtonGroup* closeActionGroup_ = nullptr;
    QPushButton* factionButton_ = nullptr;
    QSlider* statusOverlayOpacitySlider_ = nullptr;
    QLabel* statusOverlayOpacityValue_ = nullptr;
    QLabel* factionPreviewIcon_ = nullptr;
    QLabel* factionPreviewName_ = nullptr;
    QWidget* factionPreviewBar_ = nullptr;
    FluentSwitch* backgroundAutoRunSwitch_ = nullptr;
    QRadioButton* radioAsk_ = nullptr;
    QRadioButton* radioMin_ = nullptr;
    QRadioButton* radioExit_ = nullptr;
};
