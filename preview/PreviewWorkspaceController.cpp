#include "PreviewWorkspaceController.h"
#include <QStringList>

#include <algorithm>

PreviewWorkspaceController::PreviewWorkspaceController(bool macro, PreviewDialogModel& dialog, QObject* parent)
    : QObject(parent), model_(macro), dialog_(dialog) {
    connect(&model_, &PreviewWorkspaceModel::requested, this, &PreviewWorkspaceController::handle);
}

void PreviewWorkspaceController::handle(const QString& action, const QVariant& argument) {
    if (action == "add" || action == "edit") {
        editRow(argument, action == "add");
    } else if (action == "insertBefore" || action == "insertAfter") {
        const int index = rowIndexById(argument.toString());
        if (index >= 0) editRow({}, true, action == "insertBefore" ? index : index + 1, true);
    } else if (action == "duplicate") {
        const int index = rowIndexById(argument.toString());
        if (index < 0) return;
        auto rows = model_.rows();
        auto copied = rows[index].toMap();
        const QString createdId = QString::number(nextId_++);
        copied.insert("id", createdId);
        rows.insert(index + 1, copied);
        model_.setRows(rows);
        model_.focusSteps({createdId});
    } else if (action == "remove") {
        dialog_.present("DialogMessage.qml", tr("删除按键"), {{"message", tr("确认删除这个示例按键？")}},
            [this, argument](const QVariantMap&) {
                auto rows = model_.rows();
                const int index = [&]() {
                    for (int i = 0; i < rows.size(); ++i)
                        if (rows[i].toMap().value("id").toString() == argument.toString()) return i;
                    return -1;
                }();
                if (index >= 0 && index < rows.size()) rows.removeAt(index);
                model_.setRows(rows);
            }, 360, true);
    } else if (action == "batchDuplicate") {
        const auto selected = argument.toList();
        if (selected.isEmpty()) return;
        auto rows = model_.rows();
        QVariantList copies;
        int lastIndex = -1;
        for (int index = 0; index < rows.size(); ++index) {
            if (!selected.contains(rows[index].toMap().value("id"))) continue;
            auto copied = rows[index].toMap();
            copied.insert("id", QString::number(nextId_++));
            copies.push_back(copied);
            lastIndex = index;
        }
        if (copies.isEmpty()) return;
        for (int index = 0; index < copies.size(); ++index)
            rows.insert(lastIndex + 1 + index, copies[index]);
        model_.setRows(rows);
        QStringList createdIds;
        for (const auto& copy : copies) createdIds.push_back(copy.toMap().value("id").toString());
        model_.selectSteps(createdIds);
    } else if (action == "batchDelete") {
        const auto selected = argument.toList();
        if (selected.isEmpty()) return;
        dialog_.present("DialogMessage.qml", model_.macroMode() ? tr("批量删除步骤") : tr("批量删除按键"),
            {{"message", model_.macroMode() ? tr("确认删除选中的 %1 个示例步骤？").arg(selected.size())
                : tr("确认删除选中的 %1 个示例按键？").arg(selected.size())}},
            [this, selected](const QVariantMap&) {
                QVariantList rows;
                for (const auto& value : model_.rows()) {
                    if (!selected.contains(value.toMap().value("id"))) rows.append(value);
                }
                model_.setRows(rows);
                if (model_.macroMode()) model_.clearStepSelection();
                else model_.clearBindingSelection();
            }, 360, true);
    } else if (action == "batchEdit") {
        editRows(argument.toList());
    } else if (action == "emptyMacro") {
        dialog_.message(tr("无法启动"), tr("请先启用至少一个循环步骤。"));
    } else if (action.startsWith("profile")) {
        editProfile(action);
    } else if (action == "hotkey" || action == "sequence" || action == "pause") {
        editShortcut(action);
    } else if (action == "sound") {
        dialog_.present("DialogSound.qml", tr("提示音设置"), sound_,
            [this](const QVariantMap& values) { sound_ = values; });
    } else if (action == "overlay") {
        emit overlaySettingsRequested();
    } else if (action == "update") {
        emit aboutRequested();
    } else if (action == "share") {
        QStringList names;
        for (const auto& value : model_.rows()) {
            const auto row = value.toMap();
            if (!row.value("name").toString().isEmpty())
                names << tr("%1：%2").arg(row.value("keyText").toString(), row.value("name").toString());
        }
        dialog_.present("DialogShare.qml", tr("导出配置"), {{"exporting", true}, {"format", 1},
            {"namePreview", names.join(QStringLiteral("\n"))},
            {"code", tr("【界面预览示例】\n默认配置\n按键 1：开关模式，10ms\n按键 侧键1：按压模式，10ms")},
            {"status", tr("此处展示导出窗口，内容为预览示例。")}});
    }
}

void PreviewWorkspaceController::editRows(const QVariantList& selected) {
    if (selected.isEmpty()) return;
    QVariantMap fields{{"macroMode", model_.macroMode()}, {"count", selected.size()},
        {"modifyDelay", false}, {"delay", QStringLiteral("10")},
        {"modifyEnabled", false}, {"enabled", true},
        {"modifyMode", false}, {"mode", 0},
        {"modifyPriority", false}, {"priority", true}};
    dialog_.present("DialogBatchEdit.qml",
        model_.macroMode() ? tr("批量修改步骤") : tr("批量修改按键"), fields,
        [this, selected](const QVariantMap& values) {
            auto rows = model_.rows();
            for (auto& value : rows) {
                auto row = value.toMap();
                if (!selected.contains(row.value("id"))) continue;

                if (values.value("modifyEnabled").toBool())
                    row.insert("enabled", values.value("enabled").toBool());

                if (model_.macroMode()) {
                    if (values.value("modifyDelay").toBool())
                        row.insert("delay", std::max(1, values.value("delay").toString().toInt()));
                    value = row;
                    continue;
                }

                const int originalMode = row.value("mode").toInt();
                const int mode = values.value("modifyMode").toBool()
                    ? values.value("mode").toInt() : originalMode;
                if (values.value("modifyMode").toBool()) {
                    static const QStringList modes{tr("开关"), tr("按压"), tr("独立"), tr("切换"), tr("长按")};
                    row.insert("mode", mode);
                    row.insert("modeText", modes[mode]);
                }
                if (values.value("modifyDelay").toBool() && mode != 4)
                    row.insert("delay", std::max(1, values.value("delay").toString().toInt()));
                else if (mode == 4)
                    row.insert("delay", QVariant(QStringLiteral("—")));

                const bool participatesPriority = mode == 1
                    && (values.value("modifyPriority").toBool()
                        ? values.value("priority").toBool()
                        : originalMode == 1 && row.value("priorityKind").toString() == QStringLiteral("enabled"));
                row.insert("priorityText", mode == 1
                    ? (participatesPriority ? QStringLiteral("✓") : QStringLiteral("×")) : QStringLiteral("—"));
                row.insert("priorityKind", mode == 1 && participatesPriority
                    ? QStringLiteral("enabled") : QStringLiteral("muted"));
                value = row;
            }
            model_.setRows(rows);
        }, model_.macroMode() ? 360 : 420);
}

int PreviewWorkspaceController::rowIndexById(const QString& id) const {
    const auto rows = model_.rows();
    for (int index = 0; index < rows.size(); ++index) {
        if (rows[index].toMap().value("id").toString() == id) return index;
    }
    return -1;
}

void PreviewWorkspaceController::editRow(const QVariant& argument, bool adding, int insertIndex, bool focusCreated) {
    const auto rows = model_.rows();
    int index = adding ? -1 : rowIndexById(argument.toString());
    if (!adding && (index < 0 || index >= rows.size())) return;
    const auto row = adding ? QVariantMap{} : rows[index].toMap();
    const int mode = adding ? 0 : row.value("mode").toInt();
    QVariantMap fields{{"keyText", adding ? QStringLiteral("A") : row.value("keyText")},
        {"macroStep", model_.macroMode()},
        {"nameVisible", true}, {"name", adding ? QString() : row.value("name")},
        {"mode", mode}, {"modeVisible", !model_.macroMode()}, {"needsDelay", mode != 4},
        {"delay", mode == 4 || adding ? QStringLiteral("10") : row.value("delay").toString()},
        {"enabled", adding || row.value("enabled").toBool()},
        {"priority", row.value("priorityKind").toString() == "enabled"}, {"modeGuideVisible", false},
        {"positionVisible", true}, {"isMouse", row.value("isMouse").toBool()}, {"fixed", false},
        {"supportsPosition", true}, {"positionHint", tr("示例固定点位：50%，50%")}};
    dialog_.present("DialogKeyEditor.qml", adding ? tr("添加按键") : tr("编辑按键"), fields,
        [this, index, row, insertIndex, focusCreated](const QVariantMap& values) {
            const QStringList modes{tr("开关"), tr("按压"), tr("独立"), tr("切换"), tr("长按")};
            auto updated = row;
            const int mode = values.value("mode").toInt();
            if (index < 0) updated.insert("id", QString::number(nextId_++));
            updated.insert("keyText", values.value("keyText"));
            const QString name = values.value("name").toString().trimmed();
            updated.insert("name", name);
            updated.insert("keyDisplayText", name.isEmpty()
                ? values.value("keyText").toString()
                : QStringLiteral("%1 · %2").arg(values.value("keyText").toString(), name));
            updated.insert("enabled", values.value("enabled"));
            updated.insert("mode", mode);
            updated.insert("modeText", modes[mode]);
            updated.insert("isMouse", values.value("isMouse"));
            updated.insert("priorityText", mode == 1 ? (values.value("priority").toBool() ? "✓" : "×") : "—");
            updated.insert("priorityKind", mode == 1 && values.value("priority").toBool() ? "enabled" : "muted");
            updated.insert("delay", mode == 4 ? QVariant(QStringLiteral("—")) : QVariant(std::max(1, values.value("delay").toString().toInt())));
            updated.insert("positionHint", values.value("fixed").toBool() ? values.value("positionHint").toString() : QString());
            auto rows = model_.rows();
            if (index < 0) {
                if (insertIndex >= 0) rows.insert(insertIndex, updated);
                else rows.append(updated);
            } else rows[index] = updated;
            model_.setRows(rows);
            if (focusCreated) model_.focusSteps({updated.value("id").toString()});
        }, 420);
}

void PreviewWorkspaceController::editProfile(const QString& action) {
    auto profiles = model_.state().value("profiles").toList();
    const int index = model_.state().value("profileIndex").toInt();
    if (action == "profileDuplicate") {
        const QString initialName = profiles[index].toMap().value("name").toString() + tr("（副本）");
        dialog_.present("DialogName.qml", tr("复制配置"),
            {{"message", tr("请输入示例配置名称")}, {"name", initialName}},
            [this, profiles, index](const QVariantMap& values) mutable {
                const QString name = values.value("name").toString().trimmed();
                if (name.isEmpty()) return;
                auto copied = profiles;
                auto profile = profiles[index].toMap();
                profile.insert("id", QString::number(nextId_++));
                profile.insert("name", name);
                copied.insert(index + 1, profile);
                model_.update("profiles", copied);
                model_.update("profileIndex", index + 1);
            }, 360);
        return;
    }
    if (action == "profileDelete") {
        if (profiles.size() <= 1) {
            dialog_.message(tr("删除配置"), tr("至少保留一个示例配置。"));
            return;
        }
        dialog_.present("DialogMessage.qml", tr("删除配置"), {{"message", tr("确认删除当前示例配置？")}},
            [this, profiles, index](const QVariantMap&) mutable {
                profiles.removeAt(index);
                model_.update("profileIndex", 0);
                model_.update("profiles", profiles);
            }, 360, true);
        return;
    }
    dialog_.present("DialogName.qml", action == "profileAdd" ? tr("新增配置") : tr("重命名配置"),
        {{"message", tr("请输入示例配置名称")}, {"name", action == "profileAdd" ? tr("新配置") : profiles[index].toMap().value("name").toString()}},
        [this, action, profiles, index](const QVariantMap& values) mutable {
            const QString name = values.value("name").toString().trimmed();
            if (name.isEmpty()) return;
            if (action == "profileAdd") {
                profiles.append(QVariantMap{{"id", QString::number(nextId_++)}, {"name", name}});
                model_.update("profiles", profiles);
                model_.update("profileIndex", profiles.size() - 1);
            } else {
                auto profile = profiles[index].toMap();
                profile.insert("name", name);
                profiles[index] = profile;
                model_.update("profiles", profiles);
            }
        }, 360);
}

void PreviewWorkspaceController::editShortcut(const QString& kind) {
    dialog_.present("DialogKeyEditor.qml", tr("设置快捷键"),
        {{"keyText", QStringLiteral("F10")}, {"mode", 0}, {"modeVisible", false},
            {"needsDelay", false}, {"delay", QStringLiteral("10")}, {"modeGuideVisible", false},
            {"positionVisible", false}, {"isMouse", false}, {"enabled", true}, {"priority", false}},
        [this, kind](const QVariantMap& values) {
            const QString key = values.value("keyText").toString();
            if (kind == "hotkey") model_.update("hotkey", key);
            else if (kind == "sequence") {
                model_.update("hasSequenceKey", true);
                model_.update("sequenceKey", key);
            } else {
                auto keys = model_.state().value("pauseKeys").toList();
                if (!keys.contains(key)) keys.append(key);
                model_.update("pauseKeys", keys);
            }
        }, 360);
}
