#include "MainWindow.h"

#include "FactionTheme.h"
#include "config/ActiveConfigValidator.h"
#include "dialogs/StatusOverlaySettingsDialog.h"
#include "ui/components/FluentSidebar.h"
#include "ui/components/ShareImportBanner.h"
#include "ui/pages/DriverPage.h"
#include "ui/pages/KeyPage.h"
#include "ui/pages/MacroPage.h"
#include "ui/pages/SettingsPage.h"

#include <QApplication>
#include <QClipboard>
#include <QCloseEvent>
#include <QTimer>

void MainWindow::setupUi() {
    view_.setup(this);
    connect(view_.keyPage(), &KeyPage::statusOverlaySettingsClicked, this,
        [this]() { openStatusOverlaySettings(false); });
    connect(view_.macroPage(), &MacroPage::statusOverlaySettingsClicked, this,
        [this]() { openStatusOverlaySettings(true); });
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
        [this]() { return profileController_.activeProfile(); },
        [this]() { return macroController_.activeMacro(); },
        [this]() { view_.updateFactionVisuals(this); }
    });

    updateCoordinator_.bind(UpdateCoordinator::Context{
        view_.aboutPage(),
        view_.keyPage(),
        &updateService_,
        &config_,
        [this]() { configCoordinator_.save(); },
        [this](const QString& stagingDir) { applyStagedUpdateAndRestart(stagingDir); },
        [this]() {
            view_.sidebar()->setCurrentPage(QStringLiteral("about"));
            view_.navigateTo(QStringLiteral("about"));
        }
    });

    connect(view_.sidebar(), &FluentSidebar::pageSelected, this, [this](const QString& pageId) {
        view_.navigateTo(pageId);
        if (pageId == QStringLiteral("driver")) {
            QTimer::singleShot(0, this, [this]() { refreshDriverStatusUi(); });
        }
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
        [this]() { captureCoordinator_.startSequenceSwitchKeyCapture(); },
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
        [this]() { return activeInputDriverReady(); },
        [this]() { showDdUnavailableMessage(); },
        [this]() { return inputDriver_.SupportsMouseMove(); }
    });

    connect(view_.keyPage(), &KeyPage::shareRequested, this, [this]() {
        openExportProfileDialog();
    });
    connect(view_.macroPage(), &MacroPage::shareRequested, this, [this]() {
        openExportMacroDialog();
    });

    // 剪贴板分享码检测：提示条点击导入、关闭后不再重复提示；
    // 拖放分享文本到主窗口也可直接进入导入。
    connect(view_.shareBanner(), &ShareImportBanner::importRequested, this,
        [this](const QString& text) { openImportDialog(text); });
    connect(view_.shareBanner(), &ShareImportBanner::dismissed, this,
        [this](const QString& text) { suppressShareText(text); });
    connect(QApplication::clipboard(), &QClipboard::dataChanged, this, [this]() {
        if (isActiveWindow()) {
            checkClipboardForShareText();
        }
    });
    setAcceptDrops(true);

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
        [this]() {
            pageRefreshCoordinator_.refreshMacroProfiles();
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
        [this]() { return activeInputDriverReady(); },
        [this]() { showDdUnavailableMessage(); },
        [this]() { return inputDriver_.SupportsMouseMove(); }
    });

    settingsCoordinator_.bind(SettingsCoordinator::Context{
        view_.settingsPage(),
        &config_,
        this,
        [this]() { configCoordinator_.save(); },
        [this]() {
            view_.updateFactionVisuals(this);
            refreshStatusOverlay();
        },
        [this]() { captureCoordinator_.startMainHotkeyCapture(); },
        [this]() { applyGameGateSettings(); },
        [this]() { applyAutoStartSettings(); }
    });
    driverCoordinator_.bind(DriverCoordinator::Context{
        view_.driverPage(),
        &config_,
        &ddRuntimeService_,
        this,
        [this](DriverMode mode) { return switchDriverMode(mode); },
        [this](DriverMode mode) { prepareDriverUninstall(mode); },
        [this](DriverMode mode) { finalizeDriverUninstall(mode); },
        [this]() { return rebootComputer(); },
        [this]() { refreshDriverStatusUi(); },
        [this]() { configCoordinator_.save(); },
        [this](bool open) { inputHook_.setDialogOpen(open); },
        [this]() { driverSelfCheckCoordinator_.open(); }
    });
    driverSelfCheckCoordinator_.bind(DriverSelfCheckCoordinator::Context{
        this,
        &ddRuntimeService_,
        &config_,
        [this]() { return activeInputDriverReady(); },
        [this]() { return ddRuntimeService_.Inspect(driver_); },
        [this]() {
            DdDriver stagedDriver;
            std::wstring error;
            return loadLegacyDriverInto(stagedDriver, &error);
        },
        [this]() { return loadConfiguredDriver(true); },
        [this](DriverMode mode) { return switchDriverMode(mode); },
        [this]() { return rebootComputer(); },
        [this]() { refreshDriverStatusUi(); },
        [this]() { configCoordinator_.save(); },
        [this](bool open) { inputHook_.setDialogOpen(open); }
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
        if (config_.statusOverlayShowRepeat || !config_.statusOverlayShowMacro) {
            view_.showRepeatPage();
        } else {
            view_.showMacroPage();
        }
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
        &rawInput_,
        [this]() { engineStateCoordinator_.applyBindings(); },
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

void MainWindow::openStatusOverlaySettings(bool macroPage) {
    StatusOverlaySettingsDialog dialog(config_, macroPage, this);
    if (dialog.exec() != QDialog::Accepted) return;
    const auto settings = dialog.settings();
    config_.repeatOverlay = settings.repeat;
    config_.macroOverlay = settings.macro;
    config_.statusOverlayOpacity = settings.opacity;
    configCoordinator_.save();
    refreshStatusOverlay();
}

void MainWindow::closeEvent(QCloseEvent* event) {
    if (systemShutdownRequested_) {
        prepareForSystemShutdown();
        view_.hideTrayIcon();
        event->accept();
        return;
    }
    closeCoordinator_.handleClose(event);
}
