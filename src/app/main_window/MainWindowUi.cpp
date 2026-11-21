#include "MainWindow.h"

#include "FactionTheme.h"
#include "config/ActiveConfigValidator.h"
#include "dialogs/StatusOverlaySettingsDialog.h"
#include "dialogs/KeyOverviewDialog.h"
#include "dialogs/AppMessageDialog.h"
#include "ui/models/ShareImportBannerModel.h"
#include "ui/models/DriverPageModel.h"
#include "ui/models/KeyPageModel.h"
#include "ui/models/MacroPageModel.h"
#include "ui/models/SettingsPageModel.h"

#include <QApplication>
#include <QClipboard>
#include <QCloseEvent>
#include <QTimer>

bool MainWindow::setupUi() {
    if (!view_.setup(this, &onboardingCoordinator_)) return false;
    if (!statusOverlay_.initialize(this)) return false;
    const auto showOverview = [this](bool macroMode) {
        const QString objectName = macroMode ? QStringLiteral("macroKeyOverview") : QStringLiteral("repeatKeyOverview");
        if (auto* existing = findChild<KeyOverviewDialog*>(objectName)) {
            existing->show();
            existing->raise();
            existing->activateWindow();
            return;
        }
        PageModel* page = macroMode ? static_cast<PageModel*>(view_.macroPage()) : view_.keyPage();
        auto* dialog = new KeyOverviewDialog(*page, macroMode, this);
        connect(dialog, &KeyOverviewDialog::navigationRequested, this, [this, macroMode, page, dialog](const QVariantMap& target) {
            if (page->state().value("running").toBool()) return;
            if (macroMode) macroCoordinator_.editFromOverview(target, dialog);
            else keyBindingsCoordinator_.editFromOverview(target, dialog);
        });
        dialog->show();
    };
    connect(view_.keyPage(), &KeyPageModel::overviewRequested, this, [showOverview] { showOverview(false); });
    connect(view_.macroPage(), &MacroPageModel::overviewRequested, this, [showOverview] { showOverview(true); });
    connect(&view_, &MainWindowView::exitRequested, this, [this]() {
        if (configCoordinator_.save()) quitApplication();
    });
    connect(view_.keyPage(), &KeyPageModel::statusOverlaySettingsClicked, this,
        [this]() { openStatusOverlaySettings(false); });
    connect(view_.macroPage(), &MacroPageModel::statusOverlaySettingsClicked, this,
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
        [this](bool running, bool macroMode) { soundCoordinator_.playToggleSound(running, macroMode); },
        [this]() {
            if (!activeInputDriverReady()) { showDdUnavailableMessage(); return false; }
            const auto* profile = profileController_.activeProfile();
            if (!profile || !profile->enabled) return false;
            for (const auto& binding : profile->bindings) {
                if (!binding.enabled) continue;
                auto error = inputDriver_.KeySupportError(binding.key);
                if (error.empty() && CanUseClickPosition(binding.key) && binding.clickPos.IsFixed()
                    && !inputDriver_.SupportsMouseMove()) error = L"当前驱动不支持固定点位。";
                if (!error.empty()) {
                    AppMessageDialog::warning(this, tr("当前驱动不支持此配置"), QString::fromStdWString(error));
                    return false;
                }
            }
            return true;
        }
    });

    configCoordinator_.bind(ConfigCoordinator::Context{
        this,
        &config_,
        [this]() { engineHookCoordinator_.refreshState(); },
        [this]() {
            usageStatisticsCoordinator_.markSaved();
            onboardingCoordinator_.scheduleRefresh();
        },
        [this]() { usageStatisticsCoordinator_.recordElapsed(); }
    });
    connect(&usageStatisticsCoordinator_, &UsageStatisticsCoordinator::saveRequested, this,
        [this]() { configCoordinator_.save(false, 500); });
    connect(qApp, &QCoreApplication::aboutToQuit, this,
        [this]() { usageStatisticsCoordinator_.finish(); });

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
        this,
        view_.aboutPage(),
        view_.keyPage(),
        &updateService_,
        &config_,
        [this]() { configCoordinator_.save(); },
        [this](const QString& stagingDir) { applyStagedUpdateAndRestart(stagingDir); },
        [this]() {
            view_.navigateTo(QStringLiteral("about"));
        }
    });

    connect(&view_, &MainWindowView::pageSelected, this, [this](const QString& pageId) {
        if (onboardingCoordinator_.active()) return;
        view_.navigateTo(pageId);
        if (pageId == QStringLiteral("driver")) {
            QTimer::singleShot(0, this, [this]() { refreshDriverStatusUi(); });
        }
    });
    connect(&view_, &MainWindowView::factionSelected, this, [this](const QString& factionId) {
        settingsCoordinator_.setFaction(factionId);
    });

    keyBindingsCoordinator_.bind(KeyBindingsCoordinator::Context{
        this,
        view_.keyPage(),
        &profileController_,
        &engine_,
        [this](KeyCaptureDialogBase& dialog) {
            onboardingCoordinator_.prepareKeyEditor(dialog);
            captureCoordinator_.runKeyCaptureDialog(dialog);
        },
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
        [this]() { return activeInputDriverReady(); },
        [this]() { showDdUnavailableMessage(); },
        [this]() { return inputDriver_.SupportsMouseMove(); },
        &inputHook_
    });

    connect(view_.keyPage(), &KeyPageModel::shareRequested, this, [this]() {
        openExportProfileDialog();
    });
    connect(view_.macroPage(), &MacroPageModel::shareRequested, this, [this]() {
        openExportMacroDialog();
    });

    // 剪贴板分享码检测：提示条点击导入、关闭后不再重复提示；
    // 拖放分享文本到主窗口也可直接进入导入。
    connect(view_.shareBanner(), &ShareImportBannerModel::importRequested, this,
        [this](const QString& text) { openImportDialog(text); });
    connect(view_.shareBanner(), &ShareImportBannerModel::dismissed, this,
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

    connect(view_.macroPage(), &MacroPageModel::hotkeyEditClicked, this, [this]() {
        captureCoordinator_.startMacroHotkeyCapture();
    });
    macroCoordinator_.bind(MacroCoordinator::Context{
        this,
        view_.macroPage(),
        &macroController_,
        &macroEngine_,
        &inputHook_,
        [this](KeyCaptureDialogBase& dialog) {
            onboardingCoordinator_.prepareKeyEditor(dialog);
            captureCoordinator_.runKeyCaptureDialog(dialog);
        },
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
        [this]() { return inputDriver_.SupportsMouseMove(); },
        [this](SoundEvent event) { soundCoordinator_.playEvent(event); }
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
        &xiaojiRuntimeService_,
        this,
        [this](DriverMode mode) { return switchDriverMode(mode); },
        [this](DriverRemovalTarget target) { return prepareDriverUninstall(target); },
        [this](DriverRemovalTarget target, bool succeeded) { finalizeDriverUninstall(target, succeeded); },
        [this]() { return rebootComputer(); },
        [this]() { refreshDriverStatusUi(); },
        [this]() { configCoordinator_.save(); },
        [this](bool open) { inputHook_.setDialogOpen(open); },
        [this]() { driverSelfCheckCoordinator_.open(); },
        [this]() { driverSelfCheckCoordinator_.openDiagnostic(); }
    });
    driverSelfCheckCoordinator_.bind(DriverSelfCheckCoordinator::Context{
        this,
        &config_,
        [this]() { return activeInputDriverReady(); },
        [this]() { return activeInputDriverError(); },
        [runtimeService = ddRuntimeService_](bool ready, const std::wstring& lastError) {
            return runtimeService.Inspect(ready, lastError);
        },
        [this]() {
            auto* profile = profileController_.activeProfile();
            auto* macro = macroController_.activeMacro();
            DriverRuntimeDiagnosticState state;
            state.keyProfileEnabled = profile && profile->enabled;
            state.macroEnabled = macro && macro->enabled;
            state.keyEngineRunning = engine_.IsToolRunning();
            state.macroEngineRunning = macroEngine_.HasActiveMacro();
            state.gameGateEnabled = config_.gameGateEnabled;
            state.gameGatePaused = config_.gameGateEnabled && !gameWatcher_.IsGameForeground();
            return state;
        },
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
    onboardingCoordinator_.bind(OnboardingCoordinator::Context{
        this,
        &config_,
        &view_,
        [this]() { return engine_.IsToolRunning() || macroEngine_.HasActiveMacro(); },
        [this]() { return captureCoordinator_.captureActive(); },
        [this]() { return activeInputDriverReady(); },
        [this]() { configCoordinator_.save(); },
        [this]() { driverSelfCheckCoordinator_.open(); }
    });
    return true;
}

void MainWindow::openStatusOverlaySettings(bool macroPage) {
    StatusOverlaySettingsDialog dialog(config_, macroPage, this);
    if (dialog.exec() != QDialog::Accepted) return;
    const auto settings = dialog.settings();
    config_.repeatOverlay = settings.repeat;
    config_.macroOverlay = settings.macro;
    config_.statusOverlayOpacity = settings.opacity;
    config_.statusOverlayNativeBlur = settings.nativeBlur;
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
    if (exitRequested_) {
        event->accept();
        return;
    }
    closeCoordinator_.handleClose(event);
}
