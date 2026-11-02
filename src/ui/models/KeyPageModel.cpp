#include "KeyPageModel.h"

KeyPageModel::KeyPageModel(QObject* parent) : PageModel(parent) {
    state_ = {{"bindings", QVariantList{}}, {"profiles", QVariantList{}}, {"profileIndex", -1},
        {"profileEnabled", true}, {"note", QString()}, {"running", false}, {"hotkey", tr("未设置")},
        {"beepEnabled", true}, {"overlayEnabled", false}, {"mixedMode", false},
        {"wasdProtection", false}, {"rememberSequencePosition", true},
        {"sequenceKey", tr("未设置")}, {"hasSequenceKey", false},
        {"pauseKeys", QVariantList{}}, {"hasSequenceBindings", false}, {"updateVersion", QString()}};
}

void KeyPageModel::setBindings(const std::vector<KeyBinding>& bindings) {
    QVariantList rows;
    bool hasSequence = false;
    for (const auto& binding : bindings) {
        hasSequence |= binding.mode == RepeatMode::Sequence;
        const bool fixed = binding.key.device == InputDevice::Mouse && binding.clickPos.IsFixed();
        rows.push_back(QVariantMap{{"id", QString::number(binding.id)}, {"enabled", binding.enabled},
            {"keyText", QString::fromStdWString(GetKeyDisplayName(binding.key))},
            {"modeText", QString::fromStdWString(RepeatModeToText(binding.mode))},
            {"priorityText", binding.mode == RepeatMode::Hold
                ? (binding.suppressMixedMode ? QStringLiteral("×") : QStringLiteral("✓")) : QStringLiteral("—")},
            {"priorityKind", binding.mode == RepeatMode::Hold && !binding.suppressMixedMode
                ? QStringLiteral("enabled") : QStringLiteral("muted")},
            {"delay", binding.mode == RepeatMode::LongPress
                ? QVariant(QStringLiteral("—")) : QVariant(binding.delayMs)}, {"positionHint", fixed
                ? tr("固定点位：%1%，%2%")
                    .arg(binding.clickPos.anchorX * 100, 0, 'f', 1)
                    .arg(binding.clickPos.anchorY * 100, 0, 'f', 1) : QString()}});
    }
    state_.insert("hasSequenceBindings", hasSequence);
    setValue("bindings", rows);
}

void KeyPageModel::setRunning(bool value) {
    if (state_.value("running").toBool() != value) setValue("running", value);
}

void KeyPageModel::setSequenceSwitchKey(std::optional<KeyDescriptor> key) {
    state_.insert("hasSequenceKey", key.has_value());
    setValue("sequenceKey", key ? QString::fromStdWString(GetKeyDisplayName(*key)) : tr("未设置"));
}

void KeyPageModel::setPauseKeys(const std::vector<KeyDescriptor>& keys) {
    QVariantList values;
    for (const auto& key : keys) values.push_back(QString::fromStdWString(GetKeyDisplayName(key)));
    setValue("pauseKeys", values);
}

void KeyPageModel::setProfiles(const std::vector<std::pair<std::uint64_t, QString>>& profiles, std::uint64_t activeId) {
    QVariantList values;
    int activeIndex = -1;
    for (const auto& [id, name] : profiles) {
        if (id == activeId) activeIndex = static_cast<int>(values.size());
        values.push_back(QVariantMap{{"id", QString::number(id)}, {"name", name}});
    }
    state_.insert("profileIndex", activeIndex);
    setValue("profiles", values);
}
