#include "MacroPageModel.h"

MacroPageModel::MacroPageModel(QObject* parent) : PageModel(parent) {
    state_ = {{"steps", QVariantList{}}, {"profiles", QVariantList{}}, {"profileIndex", -1},
        {"profileEnabled", false}, {"running", false}, {"hotkey", tr("未设置")},
        {"beepEnabled", true}, {"overlayEnabled", false}, {"pauseKeys", QVariantList{}},
        {"triggerMode", 0}, {"loopCount", 0}};
}

void MacroPageModel::setSteps(const std::vector<MacroStep>& steps) {
    QVariantList rows;
    for (const auto& step : steps) {
        const bool fixed = step.key.device == InputDevice::Mouse && step.clickPos.IsFixed();
        rows.push_back(QVariantMap{{"keyText", QString::fromStdWString(GetKeyDisplayName(step.key))},
            {"delay", step.delayAfterMs}, {"positionHint", fixed
                ? tr("固定点位：%1%，%2%")
                    .arg(step.clickPos.anchorX * 100, 0, 'f', 1)
                    .arg(step.clickPos.anchorY * 100, 0, 'f', 1) : QString()}});
    }
    setValue("steps", rows);
}

void MacroPageModel::setRunning(bool value) {
    if (state_.value("running").toBool() != value) setValue("running", value);
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
