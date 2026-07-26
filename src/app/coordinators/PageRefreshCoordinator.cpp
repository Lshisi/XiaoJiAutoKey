#include "coordinators/PageRefreshCoordinator.h"

#include "FactionTheme.h"
#include "MacroEngine.h"
#include "RepeaterEngine.h"
#include "ui/components/FluentSidebar.h"
#include "ui/pages/KeyPage.h"
#include "ui/pages/MacroPage.h"
#include "ui/pages/SettingsPage.h"
#include "view/MainWindowView.h"

PageRefreshCoordinator::PageRefreshCoordinator(QObject* parent) : QObject(parent) {}

void PageRefreshCoordinator::bind(Context context) {
    context_ = std::move(context);
}

void PageRefreshCoordinator::refreshAll() {
    if (!context_.view || !context_.config || !context_.repeaterEngine) return;

    FactionTheme::instance().setCurrent(QString::fromStdString(context_.config->faction));
    if (context_.refreshFactionVisuals) context_.refreshFactionVisuals();

    auto* profile = context_.activeProfile ? context_.activeProfile() : nullptr;
    if (!profile) return;

    const QString hotkey = profile->controlHotkey.has_value()
        ? QString::fromStdWString(GetKeyDisplayName(*profile->controlHotkey))
        : QString();
    const QString hotkeyText = hotkey.isEmpty() ? QString::fromWCharArray(L"\u672a\u8bbe\u7f6e") : hotkey;

    context_.view->keyPage()->setBindings(profile->bindings);
    context_.view->keyPage()->setProfileEnabled(profile->enabled);
    context_.view->keyPage()->setRunning(context_.repeaterEngine->IsToolRunning());
    context_.view->keyPage()->setHotkeyText(hotkeyText);
    context_.view->keyPage()->setBeepEnabled(context_.config->beepEnabled);
    context_.view->keyPage()->setStatusOverlayEnabled(context_.config->statusOverlayShowRepeat);
    context_.view->keyPage()->setMixedMode(profile->mixedMode);
    context_.view->keyPage()->setWasdProtection(profile->wasdProtection);
    context_.view->keyPage()->setSequenceSwitchKey(profile->sequenceSwitchKey);
    context_.view->keyPage()->setPauseKeys(profile->pauseKeys);
    context_.view->keyPage()->updateThemeColors();

    refreshProfiles();
    refreshMacro();

    context_.view->macroPage()->setBeepEnabled(context_.config->macroBeepEnabled);
    context_.view->macroPage()->setStatusOverlayEnabled(context_.config->statusOverlayShowMacro);
    context_.view->macroPage()->updateThemeColors();

    context_.view->settingsPage()->setConfig(*context_.config);
    context_.view->settingsPage()->setHotkeyText(hotkeyText);
    context_.view->settingsPage()->setConfigPath(QString::fromStdWString(context_.configPath.wstring()));
    context_.view->settingsPage()->setCloseAction(context_.config->closeAction);
    context_.view->settingsPage()->setFaction(QString::fromStdString(context_.config->faction));
    context_.view->settingsPage()->setCustomSoundPaths(QString(), QString());
}

void PageRefreshCoordinator::refreshProfiles() {
    if (!context_.view || !context_.config) return;

    std::vector<std::pair<std::uint64_t, QString>> profiles;
    for (const auto& profile : context_.config->profiles) {
        QString name = QString::fromStdWString(profile.name);
        if (!profile.enabled) name += QString::fromWCharArray(L"\uff08\u5df2\u505c\u7528\uff09");
        profiles.emplace_back(profile.id, std::move(name));
    }
    context_.view->keyPage()->setProfiles(profiles, context_.config->activeProfileId);
}

void PageRefreshCoordinator::refreshMacro() {
    if (!context_.view || !context_.config || !context_.macroEngine) return;

    refreshMacroProfiles();

    auto* macro = context_.activeMacro ? context_.activeMacro() : nullptr;
    if (macro) {
        context_.view->macroPage()->setMacroEnabled(macro->enabled);
        context_.view->macroPage()->setSteps(macro->steps);
        context_.view->macroPage()->setPauseKeys(macro->pauseKeys);
        context_.view->macroPage()->setMode(macro->triggerMode);
        context_.view->macroPage()->setLoopCount(macro->loopCount);
        const QString hotkeyText = macro->triggerKey.has_value()
            ? QString::fromStdWString(GetKeyDisplayName(*macro->triggerKey))
            : QString::fromWCharArray(L"\u672a\u8bbe\u7f6e");
        context_.view->macroPage()->setHotkeyText(hotkeyText);
    } else {
        context_.view->macroPage()->setMacroEnabled(false);
        context_.view->macroPage()->setSteps({});
        context_.view->macroPage()->setPauseKeys({});
        context_.view->macroPage()->setMode(MacroTriggerMode::Toggle);
        context_.view->macroPage()->setLoopCount(0);
        context_.view->macroPage()->setHotkeyText(QString::fromWCharArray(L"\u672a\u8bbe\u7f6e"));
    }
}

void PageRefreshCoordinator::refreshMacroProfiles() {
    if (!context_.view || !context_.config) return;

    std::vector<std::pair<std::uint64_t, QString>> macroList;
    for (const auto& macro : context_.config->macros) {
        QString name = QString::fromStdWString(macro.name);
        if (!macro.enabled) name += QString::fromWCharArray(L"\uff08\u5df2\u505c\u7528\uff09");
        macroList.emplace_back(macro.id, std::move(name));
    }
    context_.view->macroPage()->setMacroProfiles(macroList, context_.config->activeMacroId);
}
