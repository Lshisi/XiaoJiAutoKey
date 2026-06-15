#include "MainWindow.h"

#include "FactionTheme.h"
#include "ui/components/FluentSidebar.h"
#include "ui/pages/KeyPage.h"
#include "ui/pages/MacroPage.h"
#include "ui/pages/SettingsPage.h"
#include "config/ActiveConfigValidator.h"

#include <QTimer>

void MainWindow::setupUi() {
    view_.setup(this);
    engineStateCoordinator_.bind(EngineStateCoordinator::Context{
        &view_,
        &engine_,
        &macroEngine_,
        &inputHook_,
        [this]() { return profileController_.activeProfile(); },
        [this]() { return macroController_.activeMacro(); },
        [this]() { engineHookCoordinator_.refreshState(); },
        [this]() { refreshStatusOverlay(); },
        [this](bool running, bool macroMode) { soundCoordinator_.playToggleSound(running, macroMode); }
    });

    configCoordinator_.bind(ConfigCoordinator::Context{
        this,
        &config_,
        [this]() { engineHookCoordinator_.refreshState(); }
    });

    pageRefreshCoordinator_.bind(PageRefreshCoordinator::Context{
        &view_,
        &config_,
        &engine_,
        &macroEngine_,
        configCoordinator_.configPath(),
        [this]() { return profileController_.activeProfile(); },
        [this]() { return macroController_.activeMacro(); },
        [this]() { view_.updateFactionVisuals(this); }
    });
    connect(view_.sidebar(), &FluentSidebar::pageSelected, this, [this](const QString& pageId) {
        view_.navigateTo(pageId);
    });
    connect(view_.sidebar(), &FluentSidebar::factionSelected, this, [this](const QString& factionId) {
        settingsCoordinator_.setFaction(factionId);
    });

    keyBindingsCoordinator_.bind(KeyBindingsCoordinator::Context{
        this,
        view_.keyPage(),
        &profileController_,
        &engine_,
        [this](KeyCaptureDialogBase& dialog) { captureCoordinator_.runKeyCaptureDialog(dialog); },
        [this]() { captureCoordinator_.startMainHotkeyCapture(); },
        [this](bool running) { engineStateCoordinator_.setRepeatRunning(running); },
        [this]() { engineStateCoordinator_.applyBindings(); },
        [this]() { configCoordinator_.save(); },
        [this]() {
            pageRefreshCoordinator_.refreshAll();
            refreshStatusOverlay();
        },
        [this](bool enabled) { config_.beepEnabled = enabled; },
        [this](bool enabled) { config_.statusOverlayShowRepeat = enabled; },
        [this]() { return macroController_.activeMacro(); },
        [this]() { return driver_.IsReady(); },
        [this]() { showDdUnavailableMessage(); }
    });

    profileCoordinator_.bind(ProfileCoordinator::Context{
        this,
        view_.keyPage(),
        &config_,
        &profileController_,
        &macroController_,
        &engine_,
        &macroEngine_,
        &inputHook_,
        [this](bool running) { engineStateCoordinator_.setRepeatRunning(running); },
        [this]() { engineStateCoordinator_.applyBindings(); },
        [this]() { engineStateCoordinator_.applyMacros(); },
        [this]() { configCoordinator_.save(); },
        [this]() {
            pageRefreshCoordinator_.refreshAll();
            refreshStatusOverlay();
        },
        [this]() {
            pageRefreshCoordinator_.refreshProfiles();
            refreshStatusOverlay();
        }
    });

    connect(view_.macroPage(), &MacroPage::hotkeyEditClicked, this, [this]() {
        captureCoordinator_.startMacroHotkeyCapture();
    });
    macroCoordinator_.bind(MacroCoordinator::Context{
        this,
        view_.macroPage(),
        &macroController_,
        &macroEngine_,
        &inputHook_,
        [this](KeyCaptureDialogBase& dialog) { captureCoordinator_.runKeyCaptureDialog(dialog); },
        [this]() { engineStateCoordinator_.applyMacros(); },
        [this]() { configCoordinator_.save(); },
        [this]() {
            pageRefreshCoordinator_.refreshMacro();
            refreshStatusOverlay();
        },
        [this]() { refreshStatusOverlay(); },
        [this](bool running) { engineStateCoordinator_.setRepeatRunning(running); },
        [this](bool running) { soundCoordinator_.playToggleSound(running, true); },
        [this](bool enabled) {
            config_.macroBeepEnabled = enabled;
            configCoordinator_.save();
        },
        [this](bool enabled) { config_.statusOverlayShowMacro = enabled; },
        [this]() { return profileController_.activeProfile(); },
        [this]() {
            return ActiveConfigValidator::validateActiveHotkeys(
                profileController_.activeProfile(),
                macroController_.activeMacro());
        },
        [this]() { return driver_.IsReady(); },
        [this]() { showDdUnavailableMessage(); }
    });

    settingsCoordinator_.bind(SettingsCoordinator::Context{
        view_.settingsPage(),
        &config_,
        configCoordinator_.configPath(),
        this,
        [this]() { configCoordinator_.save(); },
        [this]() {
            view_.updateFactionVisuals(this);
            refreshStatusOverlay();
        },
        [this]() { refreshStatusOverlay(); },
        [this]() { captureCoordinator_.startMainHotkeyCapture(); },
        [this](DriverMode mode) { handleDriverModeChanged(mode); },
        [this]() { refreshDriverStatusUi(); },
        [this]() { handleHidUninstallRequested(); }
    });
    connect(&statusOverlay_, &StatusOverlayWindow::positionChanged, this, [this](const QPoint& position) {
        config_.statusOverlayPositionSaved = true;
        config_.statusOverlayX = position.x();
        config_.statusOverlayY = position.y();
        configCoordinator_.save();
    });
    connect(&statusOverlay_, &StatusOverlayWindow::lockChanged, this, [this](bool locked) {
        config_.statusOverlayLocked = locked;
        configCoordinator_.save();
        refreshStatusOverlay();
    });
    connect(&statusOverlay_, &StatusOverlayWindow::keyConfigRequested, this, [this]() {
        view_.showRepeatPage();
        showNormal();
        raise();
        activateWindow();
    });

    captureCoordinator_.bind(CaptureCoordinator::Context{
        this,
        view_.keyPage(),
        view_.macroPage(),
        view_.settingsPage(),
        &profileController_,
        &macroController_,
        &inputHook_,
        [this]() { engineStateCoordinator_.applyMacros(); },
        [this]() { configCoordinator_.save(); },
        [this]() {
            pageRefreshCoordinator_.refreshAll();
            refreshStatusOverlay();
        },
        [this]() {
            pageRefreshCoordinator_.refreshMacro();
            refreshStatusOverlay();
        },
        [this]() { engineHookCoordinator_.refreshState(); },
        [this](const KeyDescriptor& key) {
            return ActiveConfigValidator::validateHotkeyCandidate(
                key,
                profileController_.activeProfile(),
                macroController_.activeMacro());
        }
    });

    soundCoordinator_.bind(SoundCoordinator::Context{
        this,
        view_.keyPage(),
        view_.macroPage(),
        &config_,
        &soundService_,
        &inputHook_,
        [this]() { return !view_.isRepeatPageActive(); },
        [this]() { configCoordinator_.save(); }
    });

    closeCoordinator_.bind(CloseCoordinator::Context{
        this,
        &view_,
        &config_,
        &inputHook_,
        [this]() { configCoordinator_.save(); },
        [this]() { hide(); }
    });

    auto* macroStateTimer = new QTimer(this);
    connect(macroStateTimer, &QTimer::timeout, this, [this]() {
        macroCoordinator_.pollRunningState();
        refreshStatusOverlay();
    });
    macroStateTimer->start(100);
}

void MainWindow::closeEvent(QCloseEvent* event) {
    closeCoordinator_.handleClose(event);
}
