#include "KeyPageModel.h"

KeyPageModel::KeyPageModel(QObject* parent) : PageModel(parent) {
    state_ = {{"bindings", QVariantList{}}, {"profiles", QVariantList{}}, {"profileIndex", -1},
        {"profileEnabled", true}, {"running", false}, {"hotkey", tr("未设置")},
        {"beepEnabled", true}, {"overlayEnabled", false}, {"mixedMode", false},
        {"wasdProtection", false}, {"rememberSequencePosition", true},
        {"sequenceKey", tr("未设置")}, {"hasSequenceKey", false},
        {"sequenceSelectionText", tr("未激活")}, {"pauseKeys", QVariantList{}},
        {"hasSequenceBindings", false}, {"updateVersion", QString()}};
    state_.insert("selectedBindingIds", QVariantList{});
}

void KeyPageModel::setBindings(const std::vector<KeyBinding>& bindings, std::uint64_t profileId) {
    if (bindingsProfileId_ != profileId) {
        bindingsProfileId_ = profileId;
        state_.insert("selectedBindingIds", QVariantList{});
    }

    QVariantList validSelectedIds;
    const auto selectedIds = state_.value("selectedBindingIds").toList();
    QVariantList rows;
    bool hasSequence = false;
    for (const auto& binding : bindings) {
        hasSequence |= binding.mode == RepeatMode::Sequence;
        const bool fixed = binding.key.device == InputDevice::Mouse && binding.clickPos.IsFixed();
        const std::wstring keyName = GetKeyDisplayName(binding.key);
        const std::wstring displayName = binding.name.empty()
            ? keyName : keyName + L" · " + binding.name;
        const QString id = QString::number(binding.id);
        if (selectedIds.contains(id)) validSelectedIds.push_back(id);
        rows.push_back(QVariantMap{{"id", id}, {"enabled", binding.enabled},
            {"mode", static_cast<int>(binding.mode)},
            {"keyText", QString::fromStdWString(keyName)},
            {"name", QString::fromStdWString(binding.name)},
            {"keyDisplayText", QString::fromStdWString(displayName)},
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
    state_.insert("selectedBindingIds", validSelectedIds);
    setValue("bindings", rows);
    refreshSequenceSelection();
}

void KeyPageModel::toggleBindingSelection(const QString& bindingId) {
    auto ids = state_.value("selectedBindingIds").toList();
    const int index = ids.indexOf(bindingId);
    if (index >= 0) ids.removeAt(index);
    else ids.push_back(bindingId);
    state_.insert("selectedBindingIds", ids);
    emit stateChanged();
}

void KeyPageModel::setRunning(bool value) {
    if (state_.value("running").toBool() != value) setValue("running", value);
}

void KeyPageModel::setSequenceSwitchKey(std::optional<KeyDescriptor> key) {
    state_.insert("hasSequenceKey", key.has_value());
    setValue("sequenceKey", key ? QString::fromStdWString(GetKeyDisplayName(*key)) : tr("未设置"));
}

void KeyPageModel::setSequenceSelection(std::optional<std::uint64_t> bindingId) {
    if (sequenceSelectedBindingId_ == bindingId) return;
    sequenceSelectedBindingId_ = bindingId;
    refreshSequenceSelection();
}

void KeyPageModel::refreshSequenceSelection() {
    const auto rows = state_.value("bindings").toList();
    QVariantList sequenceRows;
    for (const auto& value : rows) {
        const auto row = value.toMap();
        if (row.value("mode").toInt() == static_cast<int>(RepeatMode::Sequence)
            && row.value("enabled").toBool()) {
            sequenceRows.push_back(row);
        }
    }

    QString selectedText = tr("未激活");
    if (sequenceSelectedBindingId_.has_value()) {
        for (int index = 0; index < sequenceRows.size(); ++index) {
            const auto row = sequenceRows[index].toMap();
            if (row.value("id").toULongLong() != *sequenceSelectedBindingId_) continue;
            const QString keyText = row.value("keyText").toString();
            const QString name = row.value("name").toString();
            const QString bindingText = name.isEmpty() ? keyText : tr("%1（%2）").arg(name, keyText);
            selectedText = tr("%1/%2 · %3")
                .arg(index + 1)
                .arg(sequenceRows.size())
                .arg(bindingText);
            break;
        }
    }
    setValue("sequenceSelectionText", selectedText);
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
