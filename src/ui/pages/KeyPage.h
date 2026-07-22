#pragma once

#include "AppTypes.h"

#include <QWidget>
#include <QLabel>
#include <QCheckBox>
#include <QHBoxLayout>
#include <vector>

class FluentTable;
class FluentButton;
class FluentLineEdit;
class FluentSwitch;
class FluentComboBox;

class KeyPage : public QWidget {
    Q_OBJECT
public:
    explicit KeyPage(QWidget* parent = nullptr);

    void setBindings(const std::vector<KeyBinding>& bindings);
    void setProfileEnabled(bool enabled);
    void setRunning(bool running);
    void setHotkeyText(const QString& text);
    void setBeepEnabled(bool enabled);
    void setStatusOverlayEnabled(bool enabled);
    void setMixedMode(bool enabled);
    void setWasdProtection(bool enabled);
    void setSequenceSwitchKey(std::optional<KeyDescriptor> key, bool recording = false);
    void setPauseKeys(const std::vector<KeyDescriptor>& keys);
    void updateThemeColors();
    FluentLineEdit* hotkeyEditWidget() const { return hotkeyEdit_; }

    // Profile management
    void setProfiles(const std::vector<std::pair<std::uint64_t, QString>>& profiles, std::uint64_t activeId);

signals:
    void addRequested();
    void editRequested(std::uint64_t id);
    void deleteRequested(std::uint64_t id);
    void enableToggled(std::uint64_t id, bool enabled);
    void startStopClicked();
    void hotkeyEditClicked();
    void hotkeyClearClicked();
    void beepToggled(bool enabled);
    void statusOverlayToggled(bool enabled);
    void mixedModeToggled(bool enabled);
    void wasdProtectionToggled(bool enabled);
    void sequenceSwitchKeyAddClicked();
    void sequenceSwitchKeyDeleteClicked();
    void pauseKeyAddClicked();
    void pauseKeyDeleteClicked(int index);
    void soundSettingsClicked();

    // Profile signals
    void profileSwitched(quint64 profileId);
    void profileAddRequested();
    void profileRenameRequested();
    void profileDeleteRequested();
    void profileEnabledToggled(bool enabled);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void refreshTable();
    void updateStartBtnText();
    void refreshSequenceSwitchKey();
    void refreshPauseKeyList();
    void applyPageTheme();

    FluentTable* table_;
    FluentButton* addBtn_;
    FluentButton* startBtn_;
    FluentLineEdit* hotkeyEdit_;
    FluentButton* hotkeyClearBtn_ = nullptr;
    QCheckBox* beepCheck_;
    QCheckBox* statusOverlayCheck_ = nullptr;
    FluentButton* soundSettingsBtn_ = nullptr;
    QCheckBox* mixedModeCheck_;
    QCheckBox* wasdProtectionCheck_ = nullptr;
    QLabel* statusLabel_;
    std::vector<KeyBinding> bindings_;
    std::optional<KeyDescriptor> sequenceSwitchKey_;
    std::vector<KeyDescriptor> pauseKeys_;
    bool sequenceSwitchRecording_ = false;
    QString hotkeyName_;
    bool running_ = false;

    // Sequence switch and pause key UI
    QWidget* sequenceSwitchKeyWidget_ = nullptr;
    QHBoxLayout* sequenceSwitchKeyLayout_ = nullptr;
    QLabel* sequenceSwitchLabel_ = nullptr;
    FluentButton* sequenceSwitchAddBtn_ = nullptr;
    FluentButton* sequenceSwitchClearBtn_ = nullptr;
    QLabel* pauseKeyLabel_ = nullptr;
    QWidget* keyToolsDivider_ = nullptr;
    QWidget* pauseKeyListWidget_ = nullptr;
    QHBoxLayout* pauseKeyListLayout_ = nullptr;
    int pauseListWidth_ = -1;
    bool pauseRefreshQueued_ = false;

    // Profile UI
    FluentComboBox* profileCombo_;
    FluentButton* profileAddBtn_;
    FluentButton* profileRenameBtn_;
    FluentButton* profileDeleteBtn_;
    QLabel* profileEnabledLabel_ = nullptr;
    FluentSwitch* profileEnabledSwitch_ = nullptr;
    bool profileEnabled_ = true;
};
