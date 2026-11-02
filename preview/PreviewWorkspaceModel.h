#pragma once

#include "ui/models/PageModel.h"

class PreviewWorkspaceModel final : public PageModel {
    Q_OBJECT
public:
    explicit PreviewWorkspaceModel(bool macro, QObject* parent = nullptr);
    bool macroMode() const { return macro_; }
    void reset(bool demo);
    void update(const QString& field, const QVariant& value) { setValue(field, value); }
    void setRows(const QVariantList& rows);
    QVariantList rows() const;

    Q_INVOKABLE void selectProfile(const QString& id);
    Q_INVOKABLE void editBinding(const QString& id) { emit requested("edit", id); }
    Q_INVOKABLE void removeBinding(const QString& id) { emit requested("remove", id); }
    Q_INVOKABLE void enableBinding(const QString& id, bool enabled);
    Q_INVOKABLE void addRequested() { emit requested("add", {}); }
    Q_INVOKABLE void addStepRequested() { emit requested("add", {}); }
    Q_INVOKABLE void editStepRequested(int index) { emit requested("edit", index); }
    Q_INVOKABLE void deleteStepRequested(int index) { emit requested("remove", index); }
    Q_INVOKABLE void stepMoveRequested(int from, int to);
    Q_INVOKABLE void startStopClicked() { setValue("running", !state_.value("running").toBool()); }
    Q_INVOKABLE void profileEnabledToggled(bool enabled) { setValue("profileEnabled", enabled); }
    Q_INVOKABLE void macroEnabledToggled(bool enabled) { setValue("profileEnabled", enabled); }
    Q_INVOKABLE void beepToggled(bool enabled) { setValue("beepEnabled", enabled); }
    Q_INVOKABLE void statusOverlayToggled(bool enabled) { setValue("overlayEnabled", enabled); }
    Q_INVOKABLE void mixedModeToggled(bool enabled) { setValue("mixedMode", enabled); }
    Q_INVOKABLE void wasdProtectionToggled(bool enabled) { setValue("wasdProtection", enabled); }
    Q_INVOKABLE void rememberSequencePositionChanged(bool enabled) { setValue("rememberSequencePosition", enabled); }
    Q_INVOKABLE void modeChanged(int value) { setValue("triggerMode", value); }
    Q_INVOKABLE void loopCountChanged(int value) { setValue("loopCount", value); }
    Q_INVOKABLE void hotkeyEditClicked() { emit requested("hotkey", {}); }
    Q_INVOKABLE void hotkeyClearClicked() { setValue("hotkey", tr("未设置")); }
    Q_INVOKABLE void sequenceSwitchKeyAddClicked() { emit requested("sequence", {}); }
    Q_INVOKABLE void sequenceSwitchKeyDeleteClicked();
    Q_INVOKABLE void pauseKeyAddClicked() { emit requested("pause", {}); }
    Q_INVOKABLE void pauseKeyDeleteClicked(int index);
    Q_INVOKABLE void profileAddRequested() { emit requested("profileAdd", {}); }
    Q_INVOKABLE void profileDuplicateRequested() { emit requested("profileDuplicate", {}); }
    Q_INVOKABLE void profileRenameRequested() { emit requested("profileRename", {}); }
    Q_INVOKABLE void profileDeleteRequested() { emit requested("profileDelete", {}); }
    Q_INVOKABLE void macroAddRequested() { emit requested("profileAdd", {}); }
    Q_INVOKABLE void macroDuplicateRequested() { emit requested("profileDuplicate", {}); }
    Q_INVOKABLE void macroRenameRequested() { emit requested("profileRename", {}); }
    Q_INVOKABLE void macroDeleteRequested() { emit requested("profileDelete", {}); }
    Q_INVOKABLE void shareRequested() { emit requested("share", {}); }
    Q_INVOKABLE void soundSettingsClicked() { emit requested("sound", {}); }
    Q_INVOKABLE void statusOverlaySettingsClicked() { emit requested("overlay", {}); }
    Q_INVOKABLE void updateBadgeClicked() { emit requested("update", {}); }

signals:
    void requested(const QString& action, const QVariant& argument);

private:
    bool macro_;
};
