#pragma once

#include "PageModel.h"
#include "AppTypes.h"

class MacroPageModel final : public PageModel {
    Q_OBJECT
public:
    explicit MacroPageModel(QObject* parent = nullptr);
    void setSteps(const std::vector<MacroStep>& steps);
    void setMacroEnabled(bool value) { setValue("profileEnabled", value); }
    void setNote(const QString& value) { setValue("note", value); }
    void setRunning(bool value);
    void setHotkeyText(const QString& value) { setValue("hotkey", value); }
    void setBeepEnabled(bool value) { setValue("beepEnabled", value); }
    void setStatusOverlayEnabled(bool value) { setValue("overlayEnabled", value); }
    void setPauseKeys(const std::vector<KeyDescriptor>& keys);
    void setMode(MacroTriggerMode value) { setValue("triggerMode", static_cast<int>(value)); }
    void setLoopCount(int value) { setValue("loopCount", value); }
    void setMacroProfiles(const std::vector<std::pair<std::uint64_t, QString>>& macros, std::uint64_t activeId);
    Q_INVOKABLE void selectProfile(const QString& id) { emit macroSwitched(id.toULongLong()); }

signals:
    void addStepRequested();
    void editStepRequested(int index);
    void deleteStepRequested(int index);
    void stepMoveRequested(int from, int to);
    void macroSwitched(quint64 id);
    void macroAddRequested();
    void macroDuplicateRequested();
    void macroRenameRequested();
    void noteEditRequested();
    void macroDeleteRequested();
    void macroEnabledToggled(bool enabled);
    void shareRequested();
    void startStopClicked();
    void hotkeyEditClicked();
    void hotkeyClearClicked();
    void beepToggled(bool enabled);
    void statusOverlayToggled(bool enabled);
    void statusOverlaySettingsClicked();
    void pauseKeyAddClicked();
    void pauseKeyDeleteClicked(int index);
    void soundSettingsClicked();
    void modeChanged(int index);
    void loopCountChanged(int count);
};
