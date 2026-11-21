#include "coordinators/SoundCoordinator.h"

#include "dialogs/SoundSettingsDialog.h"
#include "input/GlobalInputHook.h"
#include "services/SoundService.h"
#include "ui/models/KeyPageModel.h"
#include "ui/models/MacroPageModel.h"
#include "RepeaterEngine.h"

#include <QDialog>

SoundCoordinator::SoundCoordinator(QObject* parent) : QObject(parent) {}

void SoundCoordinator::bind(Context context) {
    context_ = std::move(context);
    if (context_.keyPage) {
        connect(context_.keyPage, &KeyPageModel::soundSettingsClicked, this, [this] { openSettings(SoundGroup::Repeat); });
    }
    if (context_.macroPage) {
        connect(context_.macroPage, &MacroPageModel::soundSettingsClicked, this, [this] { openSettings(SoundGroup::Macro); });
    }
}

void SoundCoordinator::playEvent(SoundEvent event) const {
    if (!context_.config || !context_.service) return;
    auto settings = context_.config->sounds[SoundEventIndex(event)];
    const bool groupEnabled = SoundDefinition(event).group == SoundGroup::Repeat
        ? context_.config->beepEnabled : context_.config->macroBeepEnabled;
    settings.enabled = settings.enabled && groupEnabled;
    context_.service->playEvent(event, settings);
}

void SoundCoordinator::playToggleSound(bool running, bool macroMode) const {
    playEvent(macroMode ? (running ? SoundEvent::MacroOn : SoundEvent::MacroOff)
                       : (running ? SoundEvent::RepeatOn : SoundEvent::RepeatOff));
}

void SoundCoordinator::handleRepeatInput(const RepeaterStatus& before, const RepeaterStatus& after) const {
    if (!before.running || !after.running) return;
    if (before.independentKey != after.independentKey) {
        playEvent(!after.independentKey ? SoundEvent::IndependentOff
            : before.independentKey ? SoundEvent::IndependentSwitch : SoundEvent::IndependentOn);
    }
    if (before.sequenceTaskId != after.sequenceTaskId && after.sequenceTaskId) playEvent(SoundEvent::SequenceSwitch);
}

void SoundCoordinator::openSettings(SoundGroup group) {
    if (!context_.config || !context_.service || !context_.inputHook) return;
    SoundSettingsDialog dialog(*context_.service, *context_.config, group, context_.parent);
    context_.inputHook->setDialogOpen(true);
    const int result = dialog.exec();
    context_.inputHook->setDialogOpen(false);
    context_.service->closeAlias(L"autokey_preview_sound");

    if (result != QDialog::Accepted) return;

    context_.config->sounds = dialog.settings();
    context_.service->warmUpSounds(context_.config->sounds);
    if (context_.saveConfig) context_.saveConfig();
}
