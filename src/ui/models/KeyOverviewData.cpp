#include "KeyOverviewData.h"

#include <QObject>
#include <QStringList>
#include <algorithm>
#include <map>

namespace {

QString KeyId(int device, quint32 code) {
    return QStringLiteral("%1:%2").arg(device).arg(code);
}

QVariantMap LayoutKey(quint32 code, const QString& label, double column, double row,
    double width = 1, double height = 1) {
    QString text = label;
    if (code == 0x90) text = QStringLiteral("Num Lock");
    else if ((code >= 0x60 && code <= 0x6f) || code == kNumpadEnterKeyCode)
        text = QStringLiteral("Num %1").arg(label);
    else if (code == 0xa0 || code == 0xa2 || code == 0xa4 || code == 0x5b) text = QObject::tr("左 %1").arg(label);
    else if (code == 0xa1 || code == 0xa3 || code == 0xa5 || code == 0x5c) text = QObject::tr("右 %1").arg(label);
    return {{"id", KeyId(0, code)}, {"label", label}, {"text", text}, {"device", 0}, {"code", code},
        {"column", column}, {"row", row}, {"units", width}, {"rows", height}, {"wheel", 0}};
}

QVariantList Keyboard() {
    QVariantList keys{LayoutKey(0x1b, QStringLiteral("Esc"), 0, 0)};
    for (int index = 0; index < 12; ++index) {
        keys.push_back(LayoutKey(0x70 + index, QStringLiteral("F%1").arg(index + 1),
            2 + index + (index / 4) * 0.5, 0));
    }
    keys += {LayoutKey(0x2c, QStringLiteral("PrtSc"), 15.5, 0),
        LayoutKey(0x91, QStringLiteral("ScrLk"), 16.5, 0), LayoutKey(0x13, QStringLiteral("Pause"), 17.5, 0)};
    keys.push_back(LayoutKey(0xc0, QStringLiteral("`"), 0, 1.25));
    for (int index = 0; index < 10; ++index) keys.push_back(LayoutKey(
        index == 9 ? 0x30 : 0x31 + index, QString::number((index + 1) % 10), index + 1, 1.25));
    keys += {LayoutKey(0xbd, QStringLiteral("−"), 11, 1.25), LayoutKey(0xbb, QStringLiteral("="), 12, 1.25),
        LayoutKey(8, QStringLiteral("Backspace"), 13, 1.25, 2), LayoutKey(9, QStringLiteral("Tab"), 0, 2.25, 1.5)};
    const QString top = QStringLiteral("QWERTYUIOP");
    for (int index = 0; index < top.size(); ++index)
        keys.push_back(LayoutKey(top[index].unicode(), top.mid(index, 1), 1.5 + index, 2.25));
    keys += {LayoutKey(0xdb, QStringLiteral("["), 11.5, 2.25), LayoutKey(0xdd, QStringLiteral("]"), 12.5, 2.25),
        LayoutKey(0xdc, QStringLiteral("\\"), 13.5, 2.25, 1.5), LayoutKey(0x14, QStringLiteral("Caps"), 0, 3.25, 1.75)};
    const QString middle = QStringLiteral("ASDFGHJKL");
    for (int index = 0; index < middle.size(); ++index)
        keys.push_back(LayoutKey(middle[index].unicode(), middle.mid(index, 1), 1.75 + index, 3.25));
    keys += {LayoutKey(0xba, QStringLiteral(";"), 10.75, 3.25), LayoutKey(0xde, QStringLiteral("'"), 11.75, 3.25),
        LayoutKey(13, QStringLiteral("Enter"), 12.75, 3.25, 2.25), LayoutKey(0xa0, QStringLiteral("Shift"), 0, 4.25, 2.25)};
    const QString bottom = QStringLiteral("ZXCVBNM");
    for (int index = 0; index < bottom.size(); ++index)
        keys.push_back(LayoutKey(bottom[index].unicode(), bottom.mid(index, 1), 2.25 + index, 4.25));
    keys += {LayoutKey(0xbc, QStringLiteral(","), 9.25, 4.25), LayoutKey(0xbe, QStringLiteral("."), 10.25, 4.25),
        LayoutKey(0xbf, QStringLiteral("/"), 11.25, 4.25), LayoutKey(0xa1, QStringLiteral("Shift"), 12.25, 4.25, 2.75),
        LayoutKey(0xa2, QStringLiteral("Ctrl"), 0, 5.25, 1.25), LayoutKey(0x5b, QStringLiteral("Win"), 1.25, 5.25, 1.25),
        LayoutKey(0xa4, QStringLiteral("Alt"), 2.5, 5.25, 1.25), LayoutKey(32, QStringLiteral("Space"), 3.75, 5.25, 6.25),
        LayoutKey(0xa5, QStringLiteral("Alt"), 10, 5.25, 1.25), LayoutKey(0x5c, QStringLiteral("Win"), 11.25, 5.25, 1.25),
        LayoutKey(0x5d, QStringLiteral("Menu"), 12.5, 5.25, 1.25), LayoutKey(0xa3, QStringLiteral("Ctrl"), 13.75, 5.25, 1.25)};
    const std::vector<std::pair<quint32, QString>> navigation{{0x2d, QStringLiteral("Ins")}, {0x24, QStringLiteral("Home")},
        {0x21, QStringLiteral("PgUp")}, {0x2e, QStringLiteral("Del")}, {0x23, QStringLiteral("End")}, {0x22, QStringLiteral("PgDn")}};
    for (int index = 0; index < 6; ++index) keys.push_back(LayoutKey(navigation[index].first,
        navigation[index].second, 15.5 + index % 3, 1.25 + index / 3));
    keys += {LayoutKey(0x26, QStringLiteral("↑"), 16.5, 4.25), LayoutKey(0x25, QStringLiteral("←"), 15.5, 5.25),
        LayoutKey(0x28, QStringLiteral("↓"), 16.5, 5.25), LayoutKey(0x27, QStringLiteral("→"), 17.5, 5.25),
        LayoutKey(0x90, QStringLiteral("Num"), 19, 1.25), LayoutKey(0x6f, QStringLiteral("/"), 20, 1.25),
        LayoutKey(0x6a, QStringLiteral("*"), 21, 1.25), LayoutKey(0x6d, QStringLiteral("−"), 22, 1.25)};
    for (int row = 0; row < 3; ++row) {
        for (int column = 0; column < 3; ++column) {
            const int number = 7 - row * 3 + column;
            keys.push_back(LayoutKey(0x60 + number, QString::number(number), 19 + column, 2.25 + row));
        }
    }
    keys += {LayoutKey(0x6b, QStringLiteral("+"), 22, 2.25, 1, 2),
        LayoutKey(kNumpadEnterKeyCode, QStringLiteral("Enter"), 22, 4.25, 1, 2),
        LayoutKey(0x60, QStringLiteral("0"), 19, 5.25, 2), LayoutKey(0x6e, QStringLiteral("."), 21, 5.25)};
    return keys;
}

QVariantList Mouse() {
    QVariantList keys;
    const std::vector<std::pair<quint32, QString>> buttons{{1, QObject::tr("左键")}, {2, QObject::tr("右键")},
        {4, QObject::tr("中键")}, {5, QObject::tr("侧键 1")}, {6, QObject::tr("侧键 2")}};
    for (const auto& [code, label] : buttons)
        keys.push_back(QVariantMap{{"id", KeyId(1, code)}, {"label", label}, {"device", 1}, {"code", code}, {"wheel", 0}});
    keys.push_back(QVariantMap{{"id", "wheel:up"}, {"label", QObject::tr("滚轮向上")}, {"wheel", 120}});
    keys.push_back(QVariantMap{{"id", "wheel:down"}, {"label", QObject::tr("滚轮向下")}, {"wheel", -120}});
    return keys;
}

QStringList PhysicalIds(int device, quint32 code) {
    if (device == 0 && code == 0x10) return {KeyId(0, 0xa0), KeyId(0, 0xa1)};
    if (device == 0 && code == 0x11) return {KeyId(0, 0xa2), KeyId(0, 0xa3)};
    if (device == 0 && code == 0x12) return {KeyId(0, 0xa4), KeyId(0, 0xa5)};
    return {KeyId(device, code)};
}

bool SameKey(const QVariantMap& left, const QVariantMap& right) {
    return !left.isEmpty() && !right.isEmpty() && left.value("device") == right.value("device")
        && left.value("code") == right.value("code") && left.value("modifiers") == right.value("modifiers");
}

}

QVariantMap KeyOverviewData::key(const KeyDescriptor& descriptor, const QString& text) {
    if (!descriptor.IsValid()) return {};
    return {{"device", static_cast<int>(descriptor.device)}, {"code", descriptor.code},
        {"modifiers", descriptor.modifiers}, {"text", text}};
}

KeyDescriptor KeyOverviewData::descriptor(const QVariantMap& value) {
    return {static_cast<InputDevice>(value.value("device").toInt()), value.value("code").toUInt(),
        static_cast<std::uint8_t>(value.value("modifiers").toUInt())};
}

QVariantMap KeyOverviewData::build(const QVariantMap& source, bool macroMode, QString& selectedId) {
    const QVariantList keyboard = Keyboard();
    const QVariantList mouse = Mouse();
    std::map<QString, QVariantMap> metadata;
    std::map<QString, QVariantList> usages;
    for (const auto& value : keyboard + mouse) metadata[value.toMap().value("id").toString()] = value.toMap();
    const bool running = source.value("running").toBool();
    const bool profileEnabled = source.value("profileEnabled").toBool();
    const auto runtime = source.value("overviewRuntime").toMap();
    const bool paused = runtime.value("paused").toBool();
    const auto attach = [&](const QVariantMap& keyData, QVariantMap usage) {
        if (keyData.isEmpty()) return;
        const int device = keyData.value("device").toInt();
        const quint32 code = keyData.value("code").toUInt();
        const auto add = [&](const QStringList& ids, bool modifier) {
            for (const auto& id : ids) {
                if (!metadata.contains(id)) {
                    metadata[id] = {{"id", id}, {"label", keyData.value("text")},
                        {"device", device}, {"code", code}, {"wheel", 0}, {"supplemental", true}};
                }
                auto entry = usage;
                entry.insert("modifier", modifier);
                if (modifier) {
                    entry.insert("kind", QStringLiteral("modifier"));
                    entry.insert("role", QObject::tr("组合键修饰 · %1").arg(usage.value("role").toString()));
                }
                usages[id].push_back(entry);
            }
        };
        add(PhysicalIds(device, code), false);
        const int modifiers = keyData.value("modifiers").toInt();
        if (modifiers & kKeyModifierCtrl) add(PhysicalIds(0, 0x11), true);
        if (modifiers & kKeyModifierShift) add(PhysicalIds(0, 0x10), true);
        if (modifiers & kKeyModifierAlt) add(PhysicalIds(0, 0x12), true);
    };
    const auto rows = source.value(macroMode ? "steps" : "bindings").toList();
    for (int index = 0; index < rows.size(); ++index) {
        const auto row = rows[index].toMap();
        const bool enabled = profileEnabled && row.value("enabled").toBool();
        const int mode = row.value("mode").toInt();
        bool active = running && enabled;
        bool rowPaused = paused;
        if (macroMode) {
            active &= runtime.value("currentStepId").toString() == row.value("id").toString();
        } else {
            if (mode == static_cast<int>(RepeatMode::Hold)) {
                active = false;
                for (const auto& keyData : runtime.value("holdKeys").toList())
                    active |= running && enabled && SameKey(row.value("key").toMap(), keyData.toMap());
                rowPaused = runtime.value("gamePaused").toBool();
            } else if (mode == static_cast<int>(RepeatMode::Independent)) {
                active &= SameKey(row.value("key").toMap(), runtime.value("independentKey").toMap());
            } else if (mode == static_cast<int>(RepeatMode::Sequence)) {
                active &= row.value("id").toString() == runtime.value("sequenceId").toString();
            }
            if (mode != static_cast<int>(RepeatMode::Hold)) rowPaused |= runtime.value("holdPriority").toBool();
        }
        const QString status = !enabled ? QObject::tr("已停用") : active
            ? (rowPaused ? QObject::tr("暂停") : QObject::tr("运行中"))
            : running ? QObject::tr("待执行") : QObject::tr("已配置");
        const QString target = QStringLiteral("output:%1").arg(row.value("id").toString());
        QString detail = macroMode ? QObject::tr("第 %1 步 · %2").arg(index + 1).arg(row.value("keyText").toString())
            : QObject::tr("%1 · %2").arg(row.value("keyText").toString(), row.value("modeText").toString());
        const QString delay = row.value("delay").toString();
        if (delay != QStringLiteral("—")) detail += QObject::tr(" · 间隔 %1ms").arg(delay);
        if (!row.value("positionHint").toString().isEmpty()) detail += QStringLiteral(" · ") + row.value("positionHint").toString();
        QVariantMap usage{{"target", target}, {"action", "output"}, {"targetId", row.value("id")},
            {"role", macroMode ? QObject::tr("循环输出") : QObject::tr("连发输出")}, {"kind", "output"},
            {"name", row.value("name")}, {"keyText", row.value("keyText")}, {"detail", detail},
            {"status", status}, {"active", active && !rowPaused}, {"paused", active && rowPaused}, {"enabled", enabled}};
        if (macroMode && row.value("action").toInt() == static_cast<int>(MacroAction::Wait)) continue;
        if (macroMode && row.value("action").toInt() == static_cast<int>(MacroAction::Wheel)) {
            const QString id = row.value("wheelDelta").toInt() < 0 ? QStringLiteral("wheel:down") : QStringLiteral("wheel:up");
            usages[id].push_back(usage);
        } else attach(row.value("key").toMap(), usage);
    }
    const auto shortcut = [&](const QVariantMap& keyData, const QString& role, const QString& kind,
        const QString& action, const QString& target, const QVariant& targetId = {}) {
        attach(keyData, {{"target", target}, {"action", action}, {"targetId", targetId},
            {"role", role}, {"kind", kind}, {"name", QString()}, {"keyText", keyData.value("text")},
            {"detail", keyData.value("text")}, {"status", profileEnabled ? QObject::tr("已配置") : QObject::tr("已停用")},
            {"active", false}, {"paused", false}, {"enabled", profileEnabled}});
    };
    shortcut(source.value("controlKey").toMap(), QObject::tr("启停键"), QStringLiteral("hotkey"), QStringLiteral("hotkey"), QStringLiteral("hotkey"));
    if (!macroMode) shortcut(source.value("sequenceKeyData").toMap(), QObject::tr("切换键"), QStringLiteral("sequence"),
        QStringLiteral("sequence"), QStringLiteral("sequence"));
    const auto pauses = source.value("pauseKeyData").toList();
    for (int index = 0; index < pauses.size(); ++index)
        shortcut(pauses[index].toMap(), QObject::tr("暂停键"), QStringLiteral("pause"), QStringLiteral("pause"), QStringLiteral("pause:%1").arg(index), index);

    QVariantMap keyStates;
    QVariantList others;
    for (const auto& [id, meta] : metadata) {
        QStringList kinds;
        QStringList roles;
        bool active = false;
        bool keyPaused = false;
        bool enabled = false;
        for (const auto& value : usages[id]) {
            const auto usage = value.toMap();
            const QString kind = usage.value("kind").toString();
            if (!kinds.contains(kind)) kinds.push_back(kind);
            if (!roles.contains(usage.value("role").toString())) roles.push_back(usage.value("role").toString());
            active |= usage.value("active").toBool();
            keyPaused |= usage.value("paused").toBool();
            enabled |= usage.value("enabled").toBool();
        }
        keyStates.insert(id, QVariantMap{{"configured", !usages[id].isEmpty()}, {"count", usages[id].size()},
            {"kind", kinds.size() == 1 ? kinds.first() : QStringLiteral("multiple")},
            {"active", active}, {"paused", keyPaused}, {"enabled", enabled},
            {"description", roles.isEmpty() ? QObject::tr("未配置") : roles.join(QObject::tr("、"))}});
        if (meta.value("supplemental").toBool() && !usages[id].isEmpty()) others.push_back(meta);
    }
    if (!metadata.contains(selectedId)) {
        selectedId = QStringLiteral("0:65");
        for (const auto& [id, entries] : usages) {
            if (!entries.isEmpty()) { selectedId = id; break; }
        }
    }
    QVariantMap selected = metadata.at(selectedId);
    const QString selectedText = selected.contains("text") ? selected.value("text").toString() : selected.value("label").toString();
    selected.insert("key", key(descriptor(selected), selectedText));
    selected.insert("usages", usages[selectedId]);
    selected.insert("canAdd", !running && (selected.value("wheel").toInt() == 0 || macroMode));
    const QString status = !profileEnabled ? QObject::tr("配置已停用") : !running ? QObject::tr("未启动")
        : paused ? QObject::tr("运行中 · 已暂停") : QObject::tr("运行中");
    return {{"keyboard", keyboard}, {"mouse", mouse}, {"otherKeys", others}, {"keyStates", keyStates},
        {"selectedKey", selected}, {"selectedId", selectedId}, {"running", running}, {"status", status},
        {"macroMode", macroMode}, {"resizable", true}};
}

QVariantMap KeyOverviewData::action(const QVariantMap& overview, const QString& targetId) {
    if (overview.value("running").toBool()) return {};
    const auto selected = overview.value("selectedKey").toMap();
    if (targetId == QStringLiteral("add") && selected.value("canAdd").toBool()) {
        auto result = selected;
        result.insert("action", QStringLiteral("add"));
        return result;
    }
    for (const auto& value : selected.value("usages").toList()) {
        const auto usage = value.toMap();
        if (usage.value("target").toString() == targetId && !usage.value("action").toString().isEmpty()) return usage;
    }
    return {};
}
