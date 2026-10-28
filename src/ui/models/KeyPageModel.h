#pragma once

#include "PageModel.h"
#include "AppTypes.h"

class KeyPageModel final : public PageModel {
    Q_OBJECT
public:
    explicit KeyPageModel(QObject* parent = nullptr);
    void setBindings(const std::vector<KeyBinding>& bindings);
    void setProfileEnabled(bool value) { setValue("profileEnabled", value); }
    void setRunning(bool value);
    void setHotkeyText(const QString& value) { setValue("hotkey", value); }
    void setBeepEnabled(bool value) { setValue("beepEnabled", value); }
    void setStatusOverlayEnabled(bool value) { setValue("overlayEnabled", value); }
    void setMixedMode(bool value) { setValue("mixedMode", value); }
    void setWasdProtection(bool value) { setValue("wasdProtection", value); }
    void setRememberSequencePosition(bool value) { setValue("rememberSequencePosition", value); }
    void setSequenceSwitchKey(std::optional<KeyDescriptor> key, bool recording = false);
    void setPauseKeys(const std::vector<KeyDescriptor>& keys);
    void setUpdateBadge(const QString& value) { setValue("updateVersion", value); }
    void setProfiles(const std::vector<std::pair<std::uint64_t, QString>>& profiles, std::uint64_t activeId);

    Q_INVOKABLE void editBinding(const QString& id) { emit editRequested(id.toULongLong()); }
    Q_INVOKABLE void removeBinding(const QString& id) { emit deleteRequested(id.toULongLong()); }
    Q_INVOKABLE void enableBinding(const QString& id, bool value) { emit enableToggled(id.toULongLong(), value); }
    Q_INVOKABLE void selectProfile(const QString& id) { emit profileSwitched(id.toULongLong()); }

signals:
    void addRequested();
    void editRequested(quint64 id);
    void deleteRequested(quint64 id);
    void enableToggled(quint64 id, bool enabled);
    void startStopClicked();
    void hotkeyEditClicked();
    void hotkeyClearClicked();
    void beepToggled(bool enabled);
    void statusOverlayToggled(bool enabled);
    void statusOverlaySettingsClicked();
    void mixedModeToggled(bool enabled);
    void wasdProtectionToggled(bool enabled);
    void rememberSequencePositionChanged(bool enabled);
    void sequenceSwitchKeyAddClicked();
    void sequenceSwitchKeyDeleteClicked();
    void pauseKeyAddClicked();
    void pauseKeyDeleteClicked(int index);
    void soundSettingsClicked();
    void updateBadgeClicked();
    void profileSwitched(quint64 id);
    void profileAddRequested();
    void profileRenameRequested();
    void profileDeleteRequested();
    void profileEnabledToggled(bool enabled);
    void shareRequested();
};
