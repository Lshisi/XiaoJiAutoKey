#pragma once

#include "PageModel.h"
#include "AppTypes.h"

class MacroPageModel final : public PageModel {
    Q_OBJECT
public:
    explicit MacroPageModel(QObject* parent = nullptr);
    void setSteps(const std::vector<MacroStep>& steps, std::uint64_t macroId);
    void selectSteps(const std::vector<std::uint64_t>& stepIds);
    void focusSteps(const std::vector<std::uint64_t>& stepIds);
    void setMacroEnabled(bool value) { setValue("profileEnabled", value); }
    void setRunning(bool value);
    void setExecutionError(const QString& value) { setValueIfChanged("executionError", value); }
    void setHotkeyText(const QString& value) { setValue("hotkey", value); }
    void setBeepEnabled(bool value) { setValue("beepEnabled", value); }
    void setStatusOverlayEnabled(bool value) { setValue("overlayEnabled", value); }
    void setPauseKeys(const std::vector<KeyDescriptor>& keys);
    void setMode(MacroTriggerMode value);
    void setLoopCount(int value);
    void setMacroProfiles(const std::vector<std::pair<std::uint64_t, QString>>& macros, std::uint64_t activeId);
    Q_INVOKABLE void selectProfile(const QString& id) { emit macroSwitched(id.toULongLong()); }
    Q_INVOKABLE void clearStepSelection() {
        state_.insert("selectedStepIds", QVariantList{});
        state_.insert("focusStepIds", QVariantList{});
        emit stateChanged();
    }
    Q_INVOKABLE void setAllStepsSelected(bool selected) {
        QVariantList ids;
        if (selected) {
            const auto steps = state_.value("steps").toList();
            for (const auto& step : steps) ids.push_back(step.toMap().value("id").toString());
        }
        state_.insert("selectedStepIds", ids);
        emit stateChanged();
    }
    Q_INVOKABLE void toggleStepSelection(const QString& stepId);
    Q_INVOKABLE void enableStep(const QString& stepId, bool enabled) { emit stepEnabledToggled(stepId, enabled); }
    Q_INVOKABLE void insertStepBeforeRequested(const QString& stepId) { emit insertStepRequested(stepId, true); }
    Q_INVOKABLE void insertStepAfterRequested(const QString& stepId) { emit insertStepRequested(stepId, false); }
    Q_INVOKABLE void duplicateStepRequested(const QString& stepId) { emit duplicateStepRequestedImpl(stepId); }
    Q_INVOKABLE void batchDuplicateRequested() { emit batchDuplicateRequestedImpl(state_.value("selectedStepIds").toList()); }
    Q_INVOKABLE void batchDeleteRequested() { emit batchDeleteRequestedImpl(state_.value("selectedStepIds").toList()); }
    Q_INVOKABLE void batchEditRequested() { emit batchEditRequestedImpl(state_.value("selectedStepIds").toList()); }

signals:
    void recordMacroRequested();
    void addStepRequested();
    void insertStepRequested(const QString& stepId, bool before);
    void editStepRequested(const QString& stepId);
    void deleteStepRequested(const QString& stepId);
    void stepEnabledToggled(const QString& stepId, bool enabled);
    void duplicateStepRequestedImpl(const QString& stepId);
    void stepMoveRequested(const QString& stepId, int targetIndex);
    void batchDuplicateRequestedImpl(const QVariantList& stepIds);
    void batchDeleteRequestedImpl(const QVariantList& stepIds);
    void batchEditRequestedImpl(const QVariantList& stepIds);
    void macroSwitched(quint64 id);
    void macroAddRequested();
    void macroDuplicateRequested();
    void macroRenameRequested();
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

private:
    static QVariantList idList(const std::vector<std::uint64_t>& ids);
    void updateTimingSummary();
    void setValueIfChanged(const QString& name, const QVariant& value);
    std::uint64_t stepsMacroId_ = 0;
    std::vector<MacroStep> steps_;
    MacroTriggerMode triggerMode_ = MacroTriggerMode::Toggle;
    int loopCount_ = 0;
};
