#include "coordinators/SoundCoordinator.h"

#include "dialogs/SoundSettingsDialog.h"
#include "input/GlobalInputHook.h"
#include "services/SoundService.h"
#include "ui/pages/KeyPage.h"
#include "ui/pages/MacroPage.h"

#include <QDialog>
#include <Windows.h>

SoundCoordinator::SoundCoordinator(QObject* parent) : QObject(parent) {}

void SoundCoordinator::bind(Context context) {
    context_ = std::move(context);
    if (context_.keyPage) {
        connect(context_.keyPage, &KeyPage::soundSettingsClicked, this, &SoundCoordinator::openSettings);
    }
    if (context_.macroPage) {
        connect(context_.macroPage, &MacroPage::soundSettingsClicked, this, &SoundCoordinator::openSettings);
    }
}

void SoundCoordinator::playToggleSound(bool running, bool macroMode) const {
    if (!context_.config || !context_.service) return;

    const auto now = static_cast<std::uint64_t>(GetTickCount64());
    if (lastToggleSoundTick_ != 0 && now - lastToggleSoundTick_ < 150) {
        return;
    }

    if (macroMode) {
        if (!context_.config->macroBeepEnabled) return;
    } else if (!context_.config->beepEnabled) {
        return;
    }

    const auto& customOn = macroMode ? context_.config->macroSoundOn : context_.config->customSoundOn;
    const auto& customOff = macroMode ? context_.config->macroSoundOff : context_.config->customSoundOff;
    const auto soundPath = context_.service->resolveToggleSoundPath(running, macroMode, customOn, customOff);
    if (soundPath.empty()) return;

    const wchar_t* alias = macroMode
        ? (running ? L"autokey_macro_on_sound" : L"autokey_macro_off_sound")
        : (running ? L"autokey_repeat_on_sound" : L"autokey_repeat_off_sound");
    context_.service->playFile(soundPath,
        macroMode ? context_.config->macroSoundVolume : context_.config->soundVolume,
        alias);
    lastToggleSoundTick_ = now;
}

void SoundCoordinator::openSettings() {
    if (!context_.config || !context_.service || !context_.inputHook) return;

    const bool macroMode = context_.macroModeActive ? context_.macroModeActive() : false;
    SoundSettingsResult initial;
    if (macroMode) {
        initial.soundOn = context_.config->macroSoundOn;
        initial.soundOff = context_.config->macroSoundOff;
        initial.volume = context_.config->macroSoundVolume;
    } else {
        initial.soundOn = context_.config->customSoundOn;
        initial.soundOff = context_.config->customSoundOff;
        initial.volume = context_.config->soundVolume;
    }

    SoundSettingsDialog dialog(*context_.service, macroMode, initial, context_.parent);
    context_.inputHook->setDialogOpen(true);
    const int result = dialog.exec();
    context_.inputHook->setDialogOpen(false);
    context_.service->closeAlias(L"autokey_preview_sound");

    if (result != QDialog::Accepted) return;

    const auto settings = dialog.settings();
    if (macroMode) {
        context_.config->macroSoundOn = settings.soundOn;
        context_.config->macroSoundOff = settings.soundOff;
        context_.config->macroSoundVolume = settings.volume;
    } else {
        context_.config->customSoundOn = settings.soundOn;
        context_.config->customSoundOff = settings.soundOff;
        context_.config->soundVolume = settings.volume;
    }
    context_.service->warmUpToggleSounds(
        context_.config->customSoundOn,
        context_.config->customSoundOff,
        context_.config->macroSoundOn,
        context_.config->macroSoundOff);
    if (context_.saveConfig) context_.saveConfig();
}
