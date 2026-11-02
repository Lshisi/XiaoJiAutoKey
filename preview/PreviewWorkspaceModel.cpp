#include "PreviewWorkspaceModel.h"
#include <QStringList>

PreviewWorkspaceModel::PreviewWorkspaceModel(bool macro, QObject* parent) : PageModel(parent), macro_(macro) {
    reset(true);
}

void PreviewWorkspaceModel::reset(bool demo) {
    const QString note = demo ? (macro_
        ? tr("基础循环示例，启动前确认游戏内 1、侧键1、F2 的键位。\n可在更多菜单中编辑或清空备注。")
        : tr("打本示例：主宏放在 1，侧键1 用于按压接管。\n切换模式对应 3；暂停键为 F9。")) : QString();
    QVariantList profiles{QVariantMap{{"id", "1"}, {"name", tr("默认配置")}, {"note", note}}};
    if (demo) profiles.append(QVariantMap{{"id", "2"}, {"name", tr("副本配置")}, {"note", QString()}});
    state_ = {{"bindings", QVariantList{}}, {"steps", QVariantList{}}, {"profiles", profiles},
        {"profileIndex", 0}, {"profileEnabled", true}, {"note", note}, {"running", false},
        {"hotkey", macro_ ? QStringLiteral("F8") : tr("侧键2")}, {"beepEnabled", true},
        {"overlayEnabled", false}, {"mixedMode", true}, {"wasdProtection", true},
        {"rememberSequencePosition", true}, {"sequenceKey", tr("未设置")},
        {"hasSequenceKey", false}, {"pauseKeys", QVariantList{}}, {"hasSequenceBindings", false},
        {"updateVersion", QString()}, {"triggerMode", 0}, {"loopCount", 0}};
    QVariantList rows;
    if (demo) {
        const QStringList keys{QStringLiteral("1"), tr("侧键1"), QStringLiteral("F2"), QStringLiteral("3"), QStringLiteral("Shift")};
        const QStringList modes{tr("开关"), tr("按压"), tr("独立"), tr("切换"), tr("长按")};
        const QStringList names{tr("主输出"), tr("接管"), tr("爆发"), tr("循环段"), QString()};
        for (int index = 0; index < (macro_ ? 3 : keys.size()); ++index) {
            const QString name = names[index];
            rows.append(QVariantMap{{"id", QString::number(index + 1)}, {"enabled", true},
                {"keyText", keys[index]}, {"name", name},
                {"keyDisplayText", name.isEmpty() ? keys[index] : QStringLiteral("%1 · %2").arg(keys[index], name)},
                {"isMouse", index == 1}, {"mode", index}, {"modeText", modes[index]},
                {"priorityText", index == 1 ? QStringLiteral("✓") : QStringLiteral("—")},
                {"priorityKind", index == 1 ? QStringLiteral("enabled") : QStringLiteral("muted")},
                {"delay", index == 4 ? QVariant(QStringLiteral("—")) : QVariant(macro_ ? 100 : 10)},
                {"positionHint", QString()}});
        }
        state_.insert("pauseKeys", QVariantList{QStringLiteral("F9")});
    }
    setRows(rows);
}

QVariantList PreviewWorkspaceModel::rows() const {
    return state_.value(macro_ ? "steps" : "bindings").toList();
}

void PreviewWorkspaceModel::update(const QString& field, const QVariant& value) {
    state_.insert(field, value);
    if (field == "profiles" || field == "profileIndex") {
        const auto profiles = state_.value("profiles").toList();
        const int index = state_.value("profileIndex").toInt();
        state_.insert("note", profiles.value(index).toMap().value("note").toString());
    }
    emit stateChanged();
}

void PreviewWorkspaceModel::setRows(const QVariantList& rows) {
    bool hasSequence = false;
    for (const auto& row : rows) hasSequence |= row.toMap().value("mode").toInt() == 3;
    state_.insert("hasSequenceBindings", hasSequence);
    setValue(macro_ ? "steps" : "bindings", rows);
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

void PreviewWorkspaceModel::stepMoveRequested(int from, int to) {
    auto steps = rows();
    if (from < 0 || to < 0 || from >= steps.size() || to >= steps.size()) return;
    steps.move(from, to);
    setRows(steps);
}

void PreviewWorkspaceModel::sequenceSwitchKeyDeleteClicked() {
    state_.insert("hasSequenceKey", false);
    setValue("sequenceKey", tr("未设置"));
}

void PreviewWorkspaceModel::pauseKeyDeleteClicked(int index) {
    auto keys = state_.value("pauseKeys").toList();
    if (index < 0 || index >= keys.size()) return;
    keys.removeAt(index);
    setValue("pauseKeys", keys);
}
