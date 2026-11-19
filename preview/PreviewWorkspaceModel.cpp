#include "PreviewWorkspaceModel.h"
#include "MacroTiming.h"
#include "MacroActions.h"
#include "RepeatModeRules.h"
#include "ui/models/KeyOverviewData.h"

#include <QStringList>
#include <algorithm>

namespace {

KeyBinding Binding(const QVariantMap& row) {
    KeyBinding binding;
    binding.key = KeyOverviewData::descriptor(row.value("key").toMap());
    binding.mode = static_cast<RepeatMode>(row.value("mode").toInt());
    binding.enabled = row.value("enabled").toBool();
    binding.suppressMixedMode = row.value("priorityKind").toString() != QStringLiteral("enabled");
    return binding;
}

QString BindingConflict(const QVariantList& rows, bool mixedMode) {
    for (int index = 0; index < rows.size(); ++index) {
        const auto first = rows[index].toMap();
        for (int otherIndex = index + 1; otherIndex < rows.size(); ++otherIndex) {
            const auto second = rows[otherIndex].toMap();
            QString reason;
            switch (CheckRepeatBindingPair(Binding(first), Binding(second), mixedMode)) {
            case RepeatBindingConflict::None: continue;
            case RepeatBindingConflict::Duplicate:
                reason = QObject::tr("同一按键和模式不能重复配置。");
                break;
            case RepeatBindingConflict::SharedTrigger:
                reason = QObject::tr("同一次物理按键不能同时作为按压触发和独立启停触发。");
                break;
            case RepeatBindingConflict::HeldTrigger:
                reason = QObject::tr("主键已由长按保持，无法再用同一物理按键可靠地触发按压或独立启停。");
                break;
            case RepeatBindingConflict::ModifierTrigger:
                reason = QObject::tr("单独修饰键的独立启停会在组合键主键按下前先触发。");
                break;
            case RepeatBindingConflict::SharedPrimary:
                reason = QObject::tr("这两种模式会同时输出同一主键，按下、松开会互相干扰。");
                break;
            case RepeatBindingConflict::PulsedModifier:
                reason = QObject::tr("修饰键的连发会打断另一项需要的按住状态；需要持续按住时请使用长按模式。");
                break;
            case RepeatBindingConflict::ModifierState:
                reason = QObject::tr("这两种模式会同时输出，但需要不同的修饰键状态。");
                break;
            }
            return QObject::tr("“%1”（%2）与“%3”（%4）冲突：%5")
                .arg(first.value("keyText").toString(), first.value("modeText").toString(),
                    second.value("keyText").toString(), second.value("modeText").toString(), reason);
        }
    }
    return {};
}

}

PreviewWorkspaceModel::PreviewWorkspaceModel(bool macro, QObject* parent) : PageModel(parent), macro_(macro) {
    reset(true);
}

void PreviewWorkspaceModel::reset(bool demo) {
    QVariantList profiles{QVariantMap{{"id", "1"}, {"name", tr("默认配置")}}};
    if (demo) profiles.append(QVariantMap{{"id", "2"}, {"name", tr("副本配置")}});
    state_ = {{"bindings", QVariantList{}}, {"steps", QVariantList{}}, {"profiles", profiles},
        {"profileIndex", 0}, {"profileEnabled", true}, {"running", false},
        {"hotkey", macro_ ? QStringLiteral("F8") : tr("侧键2")}, {"beepEnabled", true},
        {"overlayEnabled", false}, {"mixedMode", true}, {"wasdProtection", true},
        {"rememberSequencePosition", true}, {"sequenceKey", tr("未设置")},
        {"hasSequenceKey", false}, {"sequenceSelectionText", tr("未激活")},
        {"pauseKeys", QVariantList{}}, {"hasSequenceBindings", false},
        {"updateVersion", QString()}, {"triggerMode", 0}, {"loopCount", 0},
        {"selectedStepIds", QStringList{}}, {"selectedBindingIds", QStringList{}}, {"focusStepIds", QStringList{}}};
    state_.insert("controlKey", demo ? KeyOverviewData::key(
        macro_ ? KeyDescriptor{InputDevice::Keyboard, 0x77} : KeyDescriptor{InputDevice::Mouse, 6}, state_.value("hotkey").toString()) : QVariantMap{});
    if (!demo) state_.insert("hotkey", tr("未设置"));
    state_.insert("sequenceKeyData", QVariantMap{});
    state_.insert("pauseKeyData", QVariantList{});
    state_.insert("overviewPaused", false);
    state_.insert("timingText", QString());
    state_.insert("executionError", QString());
    QVariantList rows;
    if (demo) {
        const QStringList keys{QStringLiteral("Ctrl+1"), tr("侧键1"), QStringLiteral("F2"), QStringLiteral("3"), QStringLiteral("Shift")};
        const QStringList modes{tr("开关"), tr("按压"), tr("独立"), tr("切换"), tr("长按")};
        const QStringList names{tr("主输出"), tr("接管"), tr("爆发"), tr("循环段"), QString()};
        const std::vector<KeyDescriptor> descriptors{{InputDevice::Keyboard, 0x31, kKeyModifierCtrl},
            {InputDevice::Mouse, 5}, {InputDevice::Keyboard, 0x71}, {InputDevice::Keyboard, 0x33}, {InputDevice::Keyboard, 0x10}};
        for (int index = 0; index < (macro_ ? 3 : keys.size()); ++index) {
            const QString name = names[index];
            rows.append(QVariantMap{{"id", QString::number(index + 1)}, {"enabled", macro_ || index != 0},
                {"key", KeyOverviewData::key(descriptors[index], keys[index])},
                {"keyText", keys[index]}, {"name", name},
                {"keyDisplayText", name.isEmpty() ? keys[index] : QStringLiteral("%1 · %2").arg(keys[index], name)},
                {"isMouse", index == 1}, {"mode", index}, {"modeText", modes[index]},
                {"preventAccidentalStop", false},
                {"priorityText", index == 1 ? QStringLiteral("✓") : QStringLiteral("—")},
                {"priorityKind", index == 1 ? QStringLiteral("enabled") : QStringLiteral("muted")},
                {"delay", index == 4 ? QVariant(QStringLiteral("—")) : QVariant(macro_ ? 100 : 10)},
                {"action", 0}, {"duration", 500}, {"wheelDelta", 120}, {"rawKeyText", keys[index]}, {"positionHint", QString()}});
        }
        state_.insert("pauseKeys", QVariantList{QStringLiteral("F9")});
        state_.insert("pauseKeyData", QVariantList{KeyOverviewData::key({InputDevice::Keyboard, 0x78}, QStringLiteral("F9"))});
    }
    setRows(rows);
}

QVariantList PreviewWorkspaceModel::rows() const {
    return state_.value(macro_ ? "steps" : "bindings").toList();
}

void PreviewWorkspaceModel::update(const QString& field, const QVariant& value) {
    state_.insert(field, value);
    emit stateChanged();
}

void PreviewWorkspaceModel::setRows(const QVariantList& inputRows) {
    if (!macro_) {
        const auto conflict = BindingConflict(inputRows, state_.value("mixedMode").toBool());
        if (!conflict.isEmpty()) {
            emit requested("keyConflict", conflict);
            emit stateChanged();
            return;
        }
    }
    QVariantList rows = inputRows;
    if (macro_) {
        for (auto& value : rows) {
            auto row = value.toMap();
            MacroStep step;
            step.action = static_cast<MacroAction>(row.value("action").toInt());
            step.durationMs = row.value("duration", 500).toInt();
            step.wheelDelta = row.value("wheelDelta", 120).toInt();
            row.insert("keyText", QString::fromStdWString(MacroStepText(step, row.value("rawKeyText").toString().toStdWString())));
            row.insert("keyDisplayText", row.value("keyText"));
            if (step.action == MacroAction::Wait) row.insert("delay", QStringLiteral("—"));
            value = row;
        }
    }
    bool hasSequence = false;
    for (const auto& row : rows) hasSequence |= row.toMap().value("mode").toInt() == 3;
    state_.insert("hasSequenceBindings", hasSequence);
    if (macro_) {
        QStringList validSelectedIds;
        const QStringList selectedIds = state_.value("selectedStepIds").toStringList();
        for (const auto& row : rows) {
            const QString id = row.toMap().value("id").toString();
            if (selectedIds.contains(id)) validSelectedIds.append(id);
        }
        state_.insert("selectedStepIds", validSelectedIds);
    } else {
        QStringList validSelectedIds;
        const QStringList selectedIds = state_.value("selectedBindingIds").toStringList();
        for (const auto& row : rows) {
            const QString id = row.toMap().value("id").toString();
            if (selectedIds.contains(id)) validSelectedIds.append(id);
        }
        state_.insert("selectedBindingIds", validSelectedIds);
    }
    setValue(macro_ ? "steps" : "bindings", rows);
    if (macro_) refreshTiming();
    else refreshSequenceSelection();
    refreshOverviewRuntime();
}

void PreviewWorkspaceModel::selectSteps(const QStringList& ids) {
    update("selectedStepIds", ids);
    update("focusStepIds", ids);
}

void PreviewWorkspaceModel::focusSteps(const QStringList& ids) {
    update("focusStepIds", ids);
}

void PreviewWorkspaceModel::selectProfile(const QString& id) {
    const auto profiles = state_.value("profiles").toList();
    for (int index = 0; index < profiles.size(); ++index) {
        if (profiles[index].toMap().value("id").toString() == id) {
            update("profileIndex", index);
            return;
        }
    }
}

void PreviewWorkspaceModel::enableBinding(const QString& id, bool enabled) {
    auto bindings = rows();
    for (auto& binding : bindings) {
        auto row = binding.toMap();
        if (row.value("id").toString() == id) {
            row.insert("enabled", enabled);
            binding = row;
        }
    }
    setRows(bindings);
}

void PreviewWorkspaceModel::mixedModeToggled(bool enabled) {
    const auto conflict = BindingConflict(rows(), enabled);
    if (!conflict.isEmpty()) {
        emit requested("keyConflict", conflict);
        emit stateChanged();
        return;
    }
    setValue("mixedMode", enabled);
}

void PreviewWorkspaceModel::refreshSequenceSelection() {
    QVariantList sequenceRows;
    for (const auto& binding : rows()) {
        const auto row = binding.toMap();
        if (row.value("mode").toInt() == 3 && row.value("enabled").toBool()) {
            sequenceRows.append(row);
        }
    }
    if (sequenceRows.isEmpty()) {
        sequenceSelectedBindingId_.clear();
        setValue("sequenceSelectionText", tr("未激活"));
        return;
    }

    bool found = false;
    for (const auto& value : sequenceRows) {
        if (value.toMap().value("id").toString() == sequenceSelectedBindingId_) {
            found = true;
            break;
        }
    }
    if (!found) sequenceSelectedBindingId_ = sequenceRows.first().toMap().value("id").toString();

    for (int index = 0; index < sequenceRows.size(); ++index) {
        const auto row = sequenceRows[index].toMap();
        if (row.value("id").toString() != sequenceSelectedBindingId_) continue;
        const QString keyText = row.value("keyText").toString();
        const QString name = row.value("name").toString();
        const QString bindingText = name.isEmpty() ? keyText : tr("%1（%2）").arg(name, keyText);
        setValue("sequenceSelectionText", tr("%1/%2 · %3")
            .arg(index + 1)
            .arg(sequenceRows.size())
            .arg(bindingText));
        return;
    }
}

void PreviewWorkspaceModel::startStopClicked() {
    const auto steps = rows();
    if (macro_ && !state_.value("running").toBool()
        && std::none_of(steps.begin(), steps.end(), [](const QVariant& step) {
            return step.toMap().value("enabled").toBool();
        })) {
        emit requested("emptyMacro", {});
        return;
    }
    setValue("running", !state_.value("running").toBool());
    refreshOverviewRuntime();
}

void PreviewWorkspaceModel::refreshOverviewRuntime() {
    const auto entries = rows();
    QString currentStepId;
    if (macro_ && state_.value("running").toBool()) {
        for (const auto& value : entries) {
            if (value.toMap().value("enabled").toBool()) { currentStepId = value.toMap().value("id").toString(); break; }
        }
    }
    setValue("overviewRuntime", QVariantMap{{"paused", state_.value("overviewPaused")}, {"gamePaused", false}, {"holdPriority", false},
        {"holdKeys", QVariantList{}}, {"independentKey", QVariantMap{}},
        {"sequenceId", sequenceSelectedBindingId_}, {"currentStepId", currentStepId}});
}

void PreviewWorkspaceModel::modeChanged(int value) {
    state_.insert("triggerMode", value);
    refreshTiming();
    emit stateChanged();
}

void PreviewWorkspaceModel::loopCountChanged(int value) {
    state_.insert("loopCount", value);
    refreshTiming();
    emit stateChanged();
}

void PreviewWorkspaceModel::stepMoveRequested(const QString& id, int to) {
    auto steps = rows();
    int from = -1;
    for (int index = 0; index < steps.size(); ++index) {
        if (steps[index].toMap().value("id").toString() == id) {
            from = index;
            break;
        }
    }
    if (from < 0 || to < 0 || from >= steps.size() || to >= steps.size()) return;
    steps.move(from, to);
    setRows(steps);
}

void PreviewWorkspaceModel::toggleStepSelection(const QString& id) {
    QStringList ids = state_.value("selectedStepIds").toStringList();
    const int index = ids.indexOf(id);
    if (index >= 0) ids.removeAt(index);
    else ids.append(id);
    update("selectedStepIds", ids);
}

void PreviewWorkspaceModel::toggleBindingSelection(const QString& id) {
    QStringList ids = state_.value("selectedBindingIds").toStringList();
    const int index = ids.indexOf(id);
    if (index >= 0) ids.removeAt(index);
    else ids.append(id);
    update("selectedBindingIds", ids);
}

void PreviewWorkspaceModel::sequenceSwitchKeyDeleteClicked() {
    state_.insert("sequenceKeyData", QVariantMap{});
    state_.insert("hasSequenceKey", false);
    setValue("sequenceKey", tr("未设置"));
}

void PreviewWorkspaceModel::pauseKeyDeleteClicked(int index) {
    auto keys = state_.value("pauseKeys").toList();
    if (index < 0 || index >= keys.size()) return;
    keys.removeAt(index);
    auto descriptors = state_.value("pauseKeyData").toList();
    descriptors.removeAt(index);
    state_.insert("pauseKeyData", descriptors);
    setValue("pauseKeys", keys);
}

MacroConfig PreviewWorkspaceModel::timingConfig() const {
    MacroConfig macro;
    macro.triggerMode = state_.value("triggerMode").toInt() == 1
        ? MacroTriggerMode::Hold : MacroTriggerMode::Toggle;
    macro.loopCount = state_.value("loopCount").toInt();
    for (const auto& value : state_.value("steps").toList()) {
        const auto row = value.toMap();
        MacroStep step;
        step.enabled = row.value("enabled").toBool();
        step.delayAfterMs = row.value("delay").toString().toInt();
        step.action = static_cast<MacroAction>(row.value("action").toInt());
        step.durationMs = row.value("duration", 500).toInt();
        step.wheelDelta = row.value("wheelDelta", 120).toInt();
        macro.steps.push_back(std::move(step));
    }
    return macro;
}

void PreviewWorkspaceModel::refreshTiming() {
    state_.insert("timingText", QString::fromStdWString(FormatMacroTimingSummary(timingConfig())));
}
