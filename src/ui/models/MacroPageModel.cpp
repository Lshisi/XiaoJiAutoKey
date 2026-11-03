#include "MacroPageModel.h"

#include "MacroTiming.h"
#include "MacroActions.h"

MacroPageModel::MacroPageModel(QObject* parent) : PageModel(parent) {
    state_ = {{"steps", QVariantList{}}, {"profiles", QVariantList{}}, {"profileIndex", -1},
        {"profileEnabled", false}, {"running", false}, {"hotkey", tr("未设置")},
        {"beepEnabled", true}, {"overlayEnabled", false}, {"pauseKeys", QVariantList{}},
        {"triggerMode", 0}, {"loopCount", 0},
        {"selectedStepIds", QVariantList{}}, {"focusStepIds", QVariantList{}},
        {"timingText", QString()}, {"executionError", QString()}};
}

QVariantList MacroPageModel::idList(const std::vector<std::uint64_t>& ids) {
    QVariantList values;
    for (const auto id : ids) values.push_back(QString::number(id));
    return values;
}

void MacroPageModel::setSteps(const std::vector<MacroStep>& steps, std::uint64_t macroId) {
    if (stepsMacroId_ != macroId) {
        stepsMacroId_ = macroId;
        state_.insert("selectedStepIds", QVariantList{});
        state_.insert("focusStepIds", QVariantList{});
    }

    QVariantList validSelectedIds;
    const auto selectedIds = state_.value("selectedStepIds").toList();
    QVariantList rows;
    for (const auto& step : steps) {
        const bool fixed = step.key.device == InputDevice::Mouse && step.clickPos.IsFixed();
        const std::wstring& name = step.name;
        const std::wstring keyName = MacroStepText(step, MacroActionNeedsKey(step.action) ? GetKeyDisplayName(step.key) : L"");
        const std::wstring displayName = name.empty() ? keyName : keyName + L" · " + name;
        const QString id = QString::number(step.id);
        if (selectedIds.contains(id)) validSelectedIds.push_back(id);
        rows.push_back(QVariantMap{{"id", id}, {"enabled", step.enabled}, {"keyText", QString::fromStdWString(keyName)},
            {"name", QString::fromStdWString(name)},
            {"keyDisplayText", QString::fromStdWString(displayName)},
            {"delay", step.action == MacroAction::Wait ? QVariant(QStringLiteral("—")) : QVariant(step.delayAfterMs)}, {"positionHint", fixed
                ? tr("固定点位：%1%，%2%")
                    .arg(step.clickPos.anchorX * 100, 0, 'f', 1)
                    .arg(step.clickPos.anchorY * 100, 0, 'f', 1) : QString()}});
    }
    state_.insert("selectedStepIds", validSelectedIds);
    steps_ = steps;
    updateTimingSummary();
    setValue("steps", rows);
}

void MacroPageModel::selectSteps(const std::vector<std::uint64_t>& stepIds) {
    const auto ids = idList(stepIds);
    state_.insert("selectedStepIds", ids);
    state_.insert("focusStepIds", ids);
    emit stateChanged();
}

void MacroPageModel::focusSteps(const std::vector<std::uint64_t>& stepIds) {
    state_.insert("focusStepIds", idList(stepIds));
    emit stateChanged();
}

void MacroPageModel::toggleStepSelection(const QString& stepId) {
    auto ids = state_.value("selectedStepIds").toList();
    const int index = ids.indexOf(stepId);
    if (index >= 0) ids.removeAt(index);
    else ids.push_back(stepId);
    state_.insert("selectedStepIds", ids);
    emit stateChanged();
}

void MacroPageModel::setRunning(bool value) {
    if (state_.value("running").toBool() != value) setValue("running", value);
}

void MacroPageModel::setMode(MacroTriggerMode value) {
    triggerMode_ = value;
    state_.insert("triggerMode", static_cast<int>(value));
    updateTimingSummary();
    emit stateChanged();
}

void MacroPageModel::setLoopCount(int value) {
    loopCount_ = value;
    state_.insert("loopCount", value);
    updateTimingSummary();
    emit stateChanged();
}

void MacroPageModel::setPauseKeys(const std::vector<KeyDescriptor>& keys) {
    QVariantList values;
    for (const auto& key : keys) values.push_back(QString::fromStdWString(GetKeyDisplayName(key)));
    setValue("pauseKeys", values);
}

void MacroPageModel::setMacroProfiles(const std::vector<std::pair<std::uint64_t, QString>>& macros, std::uint64_t activeId) {
    QVariantList values;
    int activeIndex = -1;
    for (const auto& [id, name] : macros) {
        if (id == activeId) activeIndex = static_cast<int>(values.size());
        values.push_back(QVariantMap{{"id", QString::number(id)}, {"name", name}});
    }
    state_.insert("profileIndex", activeIndex);
    setValue("profiles", values);
}

void MacroPageModel::updateTimingSummary() {
    MacroConfig macro;
    macro.steps = steps_;
    macro.triggerMode = triggerMode_;
    macro.loopCount = loopCount_;
    setValueIfChanged("timingText", QString::fromStdWString(FormatMacroTimingSummary(macro)));
}

void MacroPageModel::setValueIfChanged(const QString& name, const QVariant& value) {
    if (state_.value(name) == value) return;
    state_.insert(name, value);
    emit stateChanged();
}
