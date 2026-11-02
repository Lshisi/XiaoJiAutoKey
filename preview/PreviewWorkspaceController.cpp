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
    } else if (action == "remove") {
        dialog_.present("DialogMessage.qml", tr("删除按键"), {{"message", tr("确认删除这个示例按键？")}},
            [this, argument](const QVariantMap&) {
                auto rows = model_.rows();
                const int index = model_.macroMode() ? argument.toInt() : [&]() {
                    for (int i = 0; i < rows.size(); ++i)
                        if (rows[i].toMap().value("id").toString() == argument.toString()) return i;
                    return -1;
                }();
                if (index >= 0 && index < rows.size()) rows.removeAt(index);
                model_.setRows(rows);
            }, 360, true);
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

void PreviewWorkspaceController::editRow(const QVariant& argument, bool adding) {
    const auto rows = model_.rows();
    int index = adding ? -1 : argument.toInt();
    if (!adding && !model_.macroMode()) {
        index = -1;
        for (int i = 0; i < rows.size(); ++i)
            if (rows[i].toMap().value("id").toString() == argument.toString()) index = i;
    }
    if (!adding && (index < 0 || index >= rows.size())) return;
    const auto row = adding ? QVariantMap{} : rows[index].toMap();
    const int mode = adding ? 0 : row.value("mode").toInt();
    QVariantMap fields{{"keyText", adding ? QStringLiteral("A") : row.value("keyText")},
        {"nameVisible", true}, {"name", adding ? QString() : row.value("name")},
        {"mode", mode}, {"modeVisible", !model_.macroMode()}, {"needsDelay", mode != 4},
        {"delay", mode == 4 || adding ? QStringLiteral("10") : row.value("delay").toString()},
        {"enabled", adding || row.value("enabled").toBool()},
        {"priority", row.value("priorityKind").toString() == "enabled"}, {"modeGuideVisible", false},
        {"positionVisible", true}, {"isMouse", row.value("isMouse").toBool()}, {"fixed", false},
        {"supportsPosition", true}, {"positionHint", tr("示例固定点位：50%，50%")}};
    dialog_.present("DialogKeyEditor.qml", adding ? tr("添加按键") : tr("编辑按键"), fields,
        [this, index, row](const QVariantMap& values) {
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
            if (index < 0) rows.append(updated);
            else rows[index] = updated;
            model_.setRows(rows);
        }, 500);
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
