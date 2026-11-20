#include "dialogs/SoundSettingsDialog.h"

#include "dialogs/AppMessageDialog.h"
#include "services/SoundService.h"
#include "ui/models/SoundSettingsData.h"
#include <QFileDialog>

SoundSettingsDialog::SoundSettingsDialog(SoundService& service, const AppConfig& config, SoundGroup group, QWidget* parent)
    : QmlDialog(parent), soundService_(service), initialSettings_(config.sounds) {
    setWindowTitle(group == SoundGroup::Repeat ? tr("按键连发提示音设置") : tr("循环连发提示音设置"));
    auto state = SoundSettingsData::build(config.sounds, group);
    state.insert("repeatGroupEnabled", config.beepEnabled);
    state.insert("macroGroupEnabled", config.macroBeepEnabled);
    state.insert("actions", QVariantList{button("accept", tr("保存")), button("cancel", tr("取消"), false)});
    refreshAvailability(state);
    present("DialogSound.qml", state, QSize(580, 410), true);
}

SoundSettings SoundSettingsDialog::settings() const { return SoundSettingsData::settings(dialogState(), initialSettings_); }

void SoundSettingsDialog::refreshAvailability(QVariantMap& state) const {
    const auto options = SoundSettingsData::settings(state, initialSettings_);
    auto events = state.value("events").toList();
    for (auto& event : events) {
        auto entry = event.toMap();
        const auto* definition = FindSoundDefinition(entry.value("id").toString().toStdString());
        if (!definition) continue;
        entry.insert("available", !soundService_.resolveSoundPath(definition->event,
            options[SoundEventIndex(definition->event)]).empty());
        event = entry;
    }
    state.insert("events", events);
}

void SoundSettingsDialog::applyState(QVariantMap state) {
    refreshAvailability(state);
    for (const auto& field : {"events", "selectedEvent", "status"}) setValue(field, state.value(field));
}

void SoundSettingsDialog::editField(const QString& field, const QVariant& argument) {
    auto state = dialogState();
    if (!SoundSettingsData::edit(state, field, argument)) return;
    soundService_.closeAlias(L"autokey_preview_sound");
    applyState(std::move(state));
}

void SoundSettingsDialog::handleAction(const QString& action, const QVariant& argument) {
    if (action == "accept") { accept(); return; }
    if (action != "chooseSound" && action != "resetSound" && action != "previewSound") {
        QmlDialog::handleAction(action, argument);
        return;
    }
    auto state = dialogState();
    const auto selected = SoundSettingsData::selected(state);
    const auto* definition = FindSoundDefinition(selected.value("id").toString().toStdString());
    if (!definition) return;
    soundService_.closeAlias(L"autokey_preview_sound");
    if (action == "chooseSound") {
        const QString path = QFileDialog::getOpenFileName(this, tr("选择%1提示音").arg(selected.value("title").toString()),
            {}, tr("音频文件 (*.mp3 *.wav *.wma *.mid *.midi)"));
        if (path.isEmpty()) return;
        if (!soundService_.isValidAudioFile(path)) {
            AppMessageDialog::warning(this, tr("格式不支持"), tr("请选择 mp3、wav、wma、mid、midi 格式的音频文件。"));
            return;
        }
        SoundSettingsData::setSelectedPath(state, path);
    } else if (action == "resetSound") SoundSettingsData::setSelectedPath(state, {});
    else {
        const auto option = settings()[SoundEventIndex(definition->event)];
        if (soundService_.resolveSoundPath(definition->event, option).empty())
            state.insert("status", tr("音频文件不存在，请放入默认文件或选择自定义音频。"));
        else {
            soundService_.playEvent(definition->event, option, true);
            state.insert("status", tr("正在试听：%1").arg(selected.value("title").toString()));
        }
    }
    applyState(std::move(state));
}
