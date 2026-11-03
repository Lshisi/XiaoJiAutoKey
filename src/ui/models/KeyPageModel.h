#pragma once

#include "PageModel.h"
#include "AppTypes.h"

class KeyPageModel final : public PageModel {
    Q_OBJECT
public:
    explicit KeyPageModel(QObject* parent = nullptr);
    void setBindings(const std::vector<KeyBinding>& bindings, std::uint64_t profileId);
    void setProfileEnabled(bool value) { setValue("profileEnabled", value); }
    void setRunning(bool value);
    void setHotkeyText(const QString& value) { setValue("hotkey", value); }
    void setBeepEnabled(bool value) { setValue("beepEnabled", value); }
    void setStatusOverlayEnabled(bool value) { setValue("overlayEnabled", value); }
    void setMixedMode(bool value) { setValue("mixedMode", value); }
    void setWasdProtection(bool value) { setValue("wasdProtection", value); }
    void setRememberSequencePosition(bool value) { setValue("rememberSequencePosition", value); }
    void setSequenceSwitchKey(std::optional<KeyDescriptor> key);
    void setSequenceSelection(std::optional<std::uint64_t> bindingId);
    void setPauseKeys(const std::vector<KeyDescriptor>& keys);
    void setUpdateBadge(const QString& value) { setValue("updateVersion", value); }
    void setProfiles(const std::vector<std::pair<std::uint64_t, QString>>& profiles, std::uint64_t activeId);

    Q_INVOKABLE void editBinding(const QString& id) { emit editRequested(id.toULongLong()); }
    Q_INVOKABLE void removeBinding(const QString& id) { emit deleteRequested(id.toULongLong()); }
    Q_INVOKABLE void enableBinding(const QString& id, bool value) { emit enableToggled(id.toULongLong(), value); }
    Q_INVOKABLE void selectProfile(const QString& id) { emit profileSwitched(id.toULongLong()); }
    Q_INVOKABLE void clearBindingSelection() {
        state_.insert("selectedBindingIds", QVariantList{});
        emit stateChanged();
    }
    Q_INVOKABLE void setAllBindingsSelected(bool selected) {
        QVariantList ids;
        if (selected) {
            const auto bindings = state_.value("bindings").toList();
            for (const auto& binding : bindings) ids.push_back(binding.toMap().value("id").toString());
        }
        state_.insert("selectedBindingIds", ids);
        emit stateChanged();
    }
    Q_INVOKABLE void toggleBindingSelection(const QString& bindingId);
    Q_INVOKABLE void batchEditRequested() { emit batchEditRequestedImpl(state_.value("selectedBindingIds").toList()); }
    Q_INVOKABLE void batchDeleteRequested() { emit batchDeleteRequestedImpl(state_.value("selectedBindingIds").toList()); }

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
    void profileDuplicateRequested();
    void profileRenameRequested();
    void profileDeleteRequested();
    void profileEnabledToggled(bool enabled);
    void shareRequested();
    void batchEditRequestedImpl(const QVariantList& bindingIds);
    void batchDeleteRequestedImpl(const QVariantList& bindingIds);

private:
    void refreshSequenceSelection();

    std::uint64_t bindingsProfileId_ = 0;
    std::optional<std::uint64_t> sequenceSelectedBindingId_;
};
