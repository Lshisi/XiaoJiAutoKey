#include "SoundSettingsData.h"

#include <QCoreApplication>
#include <algorithm>

namespace SoundSettingsData {

QVariantMap build(const SoundSettings& settings, SoundGroup group) {
    QVariantList events;
    QString selectedId;
    for (const auto& definition : SoundEventDefinitions()) {
        const auto& option = settings[SoundEventIndex(definition.event)];
        const QString id = QString::fromLatin1(definition.id);
        events.append(QVariantMap{{"id", id}, {"group", static_cast<int>(definition.group)},
            {"title", QCoreApplication::translate("SoundSettings", definition.title)},
            {"description", QCoreApplication::translate("SoundSettings", definition.description)},
            {"defaultFile", QString::fromWCharArray(definition.fileName)},
            {"customPath", QString::fromStdWString(option.customPath)},
            {"enabled", option.enabled}, {"volume", option.volume}, {"available", false}});
        if (selectedId.isEmpty() && definition.group == group) selectedId = id;
    }
    return {{"events", events}, {"group", static_cast<int>(group)}, {"selectedEvent", selectedId}, {"status", QString()}};
}

QVariantMap selected(const QVariantMap& state) {
    for (const auto& event : state.value("events").toList()) {
        const auto entry = event.toMap();
        if (entry.value("id") == state.value("selectedEvent")) return entry;
    }
    return {};
}

bool edit(QVariantMap& state, const QString& field, const QVariant& value) {
    if (field == "group") {
        const int group = value.toInt();
        if (group < 0 || group > static_cast<int>(SoundGroup::Recording)) return false;
        for (const auto& event : state.value("events").toList()) {
            const auto entry = event.toMap();
            if (entry.value("group").toInt() != group) continue;
            state.insert("group", group);
            state.insert("selectedEvent", entry.value("id"));
            state.insert("status", QString());
            return true;
        }
        return false;
    }
    auto events = state.value("events").toList();
    for (auto& event : events) {
        auto entry = event.toMap();
        if (field == "selectedEvent") {
            if (entry.value("id") != value || entry.value("group") != state.value("group")) continue;
            state.insert(field, value);
        } else {
            if (entry.value("id") != state.value("selectedEvent")) continue;
            if (field == "cueEnabled") entry.insert("enabled", value.toBool());
            else if (field == "cueVolume") entry.insert("volume", std::clamp(value.toInt(), 0, 100));
            else return false;
            event = entry;
            state.insert("events", events);
        }
        state.insert("status", QString());
        return true;
    }
    return false;
}

void setSelectedPath(QVariantMap& state, const QString& path) {
    auto events = state.value("events").toList();
    for (auto& event : events) {
        auto entry = event.toMap();
        if (entry.value("id") != state.value("selectedEvent")) continue;
        entry.insert("customPath", path);
        event = entry;
        break;
    }
    state.insert("events", events);
    state.insert("status", QString());
}

SoundSettings settings(const QVariantMap& state) {
    auto result = DefaultSoundSettings();
    for (const auto& event : state.value("events").toList()) {
        const auto entry = event.toMap();
        const auto* definition = FindSoundDefinition(entry.value("id").toString().toStdString());
        if (!definition) continue;
        result[SoundEventIndex(definition->event)] = {entry.value("enabled").toBool(),
            entry.value("customPath").toString().toStdWString(), std::clamp(entry.value("volume").toInt(), 0, 100)};
    }
    return result;
}
}
