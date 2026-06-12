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
    void setRunning(bool running);
    void setHotkeyText(const QString& text);
    void setBeepEnabled(bool enabled);
    void setStatusOverlayEnabled(bool enabled);
    void setMixedMode(bool enabled);
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
    void pauseKeyAddClicked();
    void pauseKeyDeleteClicked(int index);
    void soundSettingsClicked();

    // Profile signals
    void profileSwitched(quint64 profileId);
    void profileAddRequested();
    void profileRenameRequested();
    void profileDeleteRequested();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void refreshTable();
    void updateStartBtnText();
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
    QLabel* statusLabel_;
    std::vector<KeyBinding> bindings_;
    std::vector<KeyDescriptor> pauseKeys_;
    QString hotkeyName_;
    bool running_ = false;

    // Pause key UI
    QWidget* pauseKeyListWidget_ = nullptr;
    QHBoxLayout* pauseKeyListLayout_ = nullptr;

    // Profile UI
    FluentComboBox* profileCombo_;
    FluentButton* profileAddBtn_;
    FluentButton* profileRenameBtn_;
    FluentButton* profileDeleteBtn_;
};
