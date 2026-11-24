#include "MainWindow.h"
#include "dialogs/AppMessageDialog.h"
#include "dialogs/ConfigConfirmDialog.h"
#include "ui/models/DriverPageModel.h"
#include "ui/models/PluginCenterModel.h"
#include "ui/models/MacroPageModel.h"
#include "ui/models/SettingsPageModel.h"
#include <QScopeGuard>
#include <QCoreApplication>

namespace {
QString builtinId(DriverMode mode) {
    return mode == DriverMode::Legacy ? QStringLiteral("xiaoji.driver.dd/default")
        : mode == DriverMode::Usb ? QStringLiteral("xiaoji.driver.usb/default") : QStringLiteral("xiaoji.driver.xiaoji/default");
}
}

void MainWindow::initializePlugins() {
    pluginService_ = std::make_unique<Plugins::PluginService>(config_, QString::fromStdWString(appPaths_.exeDir()), *view_.pluginCenter(), this);
    pluginService_->setConfigWriter([this] { return configCoordinator_.save(); });
    pluginService_->setDriverRemovalProvider([this](const QString& providerId) -> Plugins::PluginService::DriverRemoval {
        if (providerId == builtinId(DriverMode::Legacy)) {
            return [service = ddRuntimeService_] {
                Plugins::PluginService::ManagementResult result;
                std::wstring message;
                result.ok = service.UninstallEmbeddedInfDriver(&message, &result.restartRequired);
                result.message = QString::fromStdWString(message);
                return result;
            };
        }
        if (providerId == builtinId(DriverMode::Xiaoji)) {
            return [service = xiaojiRuntimeService_] {
                Plugins::PluginService::ManagementResult result;
                std::wstring message;
                result.ok = service.Uninstall(&message);
                result.message = QString::fromStdWString(message);
                return result;
            };
        }
        return {};
    });
    connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit, this, [this] {
        (void)engineStateCoordinator_.setRepeatRunningFast(false);
        macroEngine_.StopAll();
        inputDriver_.ReleaseAll(); inputDriver_.ClearAllStates(); inputDriver_.SetDriver(nullptr);
        if (pluginService_) pluginService_->shutdownRuntimes();
    });
    connect(pluginService_.get(), &Plugins::PluginService::saveRequested, this, [this] { configCoordinator_.save(); });
    connect(pluginService_.get(), &Plugins::PluginService::inventoryChanged, this, &MainWindow::syncPluginInventory);
    connect(pluginService_.get(), &Plugins::PluginService::navigationRequested, this, [this](const QString& id) { view_.selectPage(id); });
    connect(pluginService_.get(), &Plugins::PluginService::driverSelectionRequested, this, [this](const QString& id) { (void)switchPluginDriver(id); });
    connect(pluginService_.get(), &Plugins::PluginService::dialogStateChanged, this, [this](bool open) { inputHook_.setDialogOpen(open); });
    connect(pluginService_.get(), &Plugins::PluginService::beforePluginDisabled, this, [this](const QString& id) {
        if (view_.currentPage().startsWith(QStringLiteral("plugin:") + id + QLatin1Char('/'))) view_.navigateTo(QStringLiteral("plugins"));
        if (!QString::fromStdString(config_.activeDriverId).startsWith(id + QLatin1Char('/'))) return;
        ++selfCheckPromptGeneration_;
        pendingSelfCheckPrompt_ = false;
        engineStateCoordinator_.setRepeatRunning(false);
        macroEngine_.StopAll();
        inputDriver_.ReleaseAll();
        inputDriver_.ClearAllStates();
        inputDriver_.SetDriver(nullptr);
        driverStatusNote_ = tr("当前驱动插件已停用或被禁用，请在插件中心查看。 ").toStdWString();
        engineHookCoordinator_.refreshState();
        refreshStatusOverlay();
    });
    connect(pluginService_.get(), &Plugins::PluginService::driverFailed, this, [this](const QString& providerId, const QString& message) {
        (void)engineStateCoordinator_.setRepeatRunningFast(false);
        macroEngine_.StopAll();
        inputDriver_.ReleaseAll();
        inputDriver_.ClearAllStates();
        inputDriver_.SetDriver(nullptr);
        if (auto* driver = pluginService_->activeDriver()) driver->disconnect();
        driverStatusNote_ = message.toStdWString();
        engineHookCoordinator_.refreshState();
        refreshStatusOverlay();
        refreshDriverStatusUi();
        if (providerId == QStringLiteral("xiaoji.driver.usb/default") && !exitRequested_ && !systemShutdownRequested_
            && pluginService_->available(providerId)) {
            usbHeartbeatReconnectPending_ = true;
            usbDeviceRefreshTimer_.start(250);
        }
    });
    connect(pluginService_.get(), &Plugins::PluginService::restartRequested, this, [this] {
        (void)engineStateCoordinator_.setRepeatRunningFast(false);
        macroEngine_.StopAll();
        inputDriver_.ReleaseAll();
        inputDriver_.SetDriver(nullptr);
        pluginService_->disconnectDrivers();
        if (!restartForDriverModeChange()) {
            loadConfiguredDriver();
            AppMessageDialog::warning(this, tr("重启失败"), tr("无法启动新实例，插件更新仍保留待重启状态。"));
        }
    });
    connect(pluginService_.get(), &Plugins::PluginService::engineRequested, this, [this](bool macro, bool running) {
        if (inputHook_.isDialogOpen() || onboardingCoordinator_.active()) return;
        if (macro) {
            if (macroEngine_.HasActiveMacro() != running) emit view_.macroPage()->startStopClicked();
        } else engineStateCoordinator_.setRepeatRunning(running);
    });
    connect(&view_, &MainWindowView::themeChanged, pluginService_.get(), &Plugins::PluginService::refreshTheme);
    connect(view_.settingsPage(), &SettingsPageModel::automaticUpdateChecksToggled, pluginService_.get(),
        [this](bool) { pluginService_->refreshBackgroundChecks(); });
    connect(view_.driverPage(), &DriverPageModel::providerSelected, this, [this](const QString& id) { (void)switchPluginDriver(id); });
    connect(view_.driverPage(), &DriverPageModel::pluginCenterRequested, this, [this] { view_.navigateTo(QStringLiteral("plugins")); });
    connect(view_.driverPage(), &DriverPageModel::pluginManagementRequested, this, [this](const QString& operation) {
        pluginService_->runManagement(QString::fromStdString(config_.activeDriverId), operation);
    });
    connect(pluginService_.get(), &Plugins::PluginService::managementStarting, this, [this](const QString& providerId, const QString& operation) {
        if (operation != QStringLiteral("uninstall")) return;
        driverUninstallInProgress_ = true;
        ++selfCheckPromptGeneration_; pendingSelfCheckPrompt_ = false;
        engineStateCoordinator_.setRepeatRunning(false);
        macroEngine_.StopAll();
        inputDriver_.ReleaseAll(); inputDriver_.ClearAllStates(); inputDriver_.SetDriver(nullptr);
        pluginService_->disconnectDrivers();
        if (providerId == builtinId(DriverMode::Legacy)) config_.ddDriverUninstalledByUser = true;
        else if (providerId == builtinId(DriverMode::Xiaoji)) config_.xiaojiDriverUninstalledByUser = true;
    });
    connect(pluginService_.get(), &Plugins::PluginService::managementFinished, this, [this](const QString& providerId, const QString& operation, bool ok) {
        if (operation == QStringLiteral("uninstall")) {
            driverUninstallInProgress_ = false;
            ++selfCheckPromptGeneration_;
            if (providerId == QString::fromStdString(config_.activeDriverId)) {
                driverUninstallFailed_ = !ok;
                driverStatusNote_ = (ok ? tr("系统驱动已卸载或等待重启，插件已保留；需要使用时请主动安装。")
                    : tr("系统驱动卸载未完成，工具已保留，请重试或主动安装/修复。")).toStdWString();
            }
        } else if (ok && (operation == QStringLiteral("install") || operation == QStringLiteral("repair"))) {
            if (providerId == builtinId(DriverMode::Legacy)) config_.ddDriverUninstalledByUser = false;
            else if (providerId == builtinId(DriverMode::Xiaoji)) config_.xiaojiDriverUninstalledByUser = false;
            configCoordinator_.save();
        }
        refreshDriverStatusUi();
    });
    connect(view_.pluginCenter(), &PluginCenterModel::stateChanged, this, [this] {
        view_.driverPage()->setPluginBusy(view_.pluginCenter()->state().value("busy").toBool());
    });
    connect(view_.driverPage(), &DriverPageModel::stateChanged, this, [this] {
        pluginService_->setHostBusy(view_.driverPage()->state().value("busy").toBool());
    });
    pluginService_->initialize();
}

void MainWindow::syncPluginInventory() {
    if (!pluginService_) return;
    const auto ddDirectory = pluginService_->pluginDirectory(QStringLiteral("xiaoji.driver.dd"));
    const auto xiaojiDirectory = pluginService_->pluginDirectory(QStringLiteral("xiaoji.driver.xiaoji"));
    ddRuntimeService_ = DdRuntimeService(ddDirectory.isEmpty() ? std::filesystem::path(appPaths_.exeDir()) : std::filesystem::path(ddDirectory.toStdWString()), appPaths_.logsDir());
    ddRuntimeService_.SetXiaojiDirectory(xiaojiDirectory.isEmpty() ? std::filesystem::path(appPaths_.exeDir()) : std::filesystem::path(xiaojiDirectory.toStdWString()));
    xiaojiRuntimeService_ = XiaojiRuntimeService(xiaojiDirectory.isEmpty() ? std::filesystem::path(appPaths_.exeDir()) : std::filesystem::path(xiaojiDirectory.toStdWString()));
    rawInput_.setPluginDeviceTokens(pluginService_->deviceTokens());
    const auto providers = pluginService_->drivers();
    const auto selected = QString::fromStdString(config_.activeDriverId);
    const auto previous = view_.driverPage()->state();
    const bool selectionChanged = previous.value("providerId").toString() != selected;
    if (selectionChanged && previous.contains("providers") && pluginService_->available(selected) && !activeInputDriverReady())
        driverStatusNote_ = tr("已选择 %1，请点击安装 / 修复或重新连接后使用。").arg(pluginService_->driverName(selected)).toStdWString();
    view_.driverPage()->setProviders(providers, selected);
    const auto ui = pluginService_->driverUi(selected);
    view_.driverPage()->setPluginManagement(ui.value("repair").toBool(), ui.value("uninstall").toBool());
    view_.driverPage()->setDiagnosticsActions(ui.value("diagnostics").toBool(), ui.value("selfCheck").toBool());
    if (selectionChanged || previous.value("providers").toList() != providers) refreshDriverStatusUi();
}

bool MainWindow::activeDriverUninstalledByUser() const {
    if (config_.activeDriverId == "xiaoji.driver.dd/default") return config_.ddDriverUninstalledByUser;
    if (config_.activeDriverId == "xiaoji.driver.xiaoji/default") return config_.xiaojiDriverUninstalledByUser;
    return false;
}

bool MainWindow::loadPluginDriver() {
    driverStatusNote_.clear();
    inputDriver_.SetDriver(nullptr);
    pluginService_->disconnectDrivers();
    const auto id = QString::fromStdString(config_.activeDriverId);
    QString error;
    if (!pluginService_->available(id, &error)) { driverStatusNote_ = error.toStdWString(); return false; }
    if (activeDriverUninstalledByUser()) { driverStatusNote_ = tr("当前系统驱动已由你主动卸载，请在驱动管理中安装或重新连接。").toStdWString(); return false; }
    if (id == builtinId(DriverMode::Xiaoji)) {
        config_.driverMode = DriverMode::Xiaoji;
        const auto status = xiaojiRuntimeService_.Inspect();
        if (!status.keyboardReady || !status.mouseReady || !status.profileReady) {
            driverStatusNote_ = tr("小叽驱动尚未就绪或输入配置需要升级，请在驱动管理中安装 / 修复。").toStdWString();
            if (!status.error.empty()) driverStatusNote_ += L"\n" + status.error;
            if (pluginService_->rollbackDriverActivation(id)) return loadPluginDriver();
            return false;
        }
    } else if (id == builtinId(DriverMode::Legacy)) {
        config_.driverMode = DriverMode::Legacy;
        if (!ensureLegacyDdInfInstalled(&driverStatusNote_)) {
            if (pluginService_->rollbackDriverActivation(id)) return loadPluginDriver();
            return false;
        }
    } else if (id == builtinId(DriverMode::Usb)) config_.driverMode = DriverMode::Usb;
    auto* driver = pluginService_->openDriver(id, &error);
    std::wstring connectionError;
    if (!driver || !connectPluginDriver(driver, id, &connectionError)) {
        driverStatusNote_ = driver ? connectionError : error.toStdWString();
        if (driverStatusNote_.empty()) driverStatusNote_ = tr("插件驱动连接失败。").toStdWString();
        if (pluginService_->rollbackDriverActivation(id)) return loadPluginDriver();
        return false;
    }
    pluginService_->confirmDriverActivation(id);
    inputDriver_.SetDriver(driver);
    if (id == builtinId(DriverMode::Usb)) { usbHeartbeatReconnectPending_ = false; usbDeviceRefreshTimer_.stop(); }
    driverUnavailableLogged_ = false;
    return true;
}

DriverCoordinator::SwitchResult MainWindow::switchPluginDriver(const QString& providerId) {
    if (!pluginService_) return DriverCoordinator::SwitchResult::Failed;
    if (pluginService_->busy()) return DriverCoordinator::SwitchResult::Canceled;
    const auto original = config_.activeDriverId;
    const auto originalMode = config_.driverMode;
    if (original == providerId.toStdString() && activeInputDriverReady()) return DriverCoordinator::SwitchResult::Succeeded;
    QString error;
    if (!pluginService_->available(providerId, &error)) {
        AppMessageDialog::warning(this, tr("驱动不可用"), error);
        return DriverCoordinator::SwitchResult::Canceled;
    }
    inputHook_.setDialogOpen(true);
    // The switch dialogs run nested event loops; catalog updates wait until it ends.
    view_.driverPage()->setBusy(true);
    const auto guard = qScopeGuard([this] { inputHook_.setDialogOpen(false); });
    const auto busyGuard = qScopeGuard([this] { view_.driverPage()->setBusy(false); });
    const auto name = pluginService_->driverName(providerId);
    if (!ConfigConfirmDialog::confirm(this, tr("切换驱动"), tr("将停止连发和循环、释放按键，准备 %1 驱动，并重启应用。").arg(name), tr("继续"))) return DriverCoordinator::SwitchResult::Canceled;
    engineStateCoordinator_.setRepeatRunning(false);
    macroEngine_.StopAll();
    inputDriver_.ReleaseAll(); inputDriver_.ClearAllStates(); inputDriver_.SetDriver(nullptr);
    pluginService_->disconnectDrivers();
    const bool wasXiaojiUninstalled = config_.xiaojiDriverUninstalledByUser;
    const bool wasDdUninstalled = config_.ddDriverUninstalledByUser;
    bool prepared = true;
    if (providerId == builtinId(DriverMode::Xiaoji)) prepared = prepareXiaojiDriverForSwitch();
    else if (providerId == builtinId(DriverMode::Legacy)) prepared = ensureLegacyDdInfInstalled(&driverStatusNote_);
    auto* candidate = prepared ? pluginService_->openDriver(providerId, &error) : nullptr;
    std::wstring connectionError;
    if (!candidate || !connectPluginDriver(candidate, providerId, &connectionError)) {
        if (candidate) error = QString::fromStdWString(connectionError);
        if (error.isEmpty()) error = QString::fromStdWString(driverStatusNote_);
        pluginService_->rollbackDriverActivation(providerId);
        if (prepared) AppMessageDialog::warning(this, tr("驱动准备失败"), error);
        loadConfiguredDriver(); refreshDriverStatusUi();
        return DriverCoordinator::SwitchResult::Canceled;
    }
    pluginService_->confirmDriverActivation(providerId);
    candidate->ReleaseAll(); candidate->disconnect();
    config_.activeDriverId = providerId.toStdString();
    if (providerId == builtinId(DriverMode::Xiaoji)) { config_.driverMode = DriverMode::Xiaoji; config_.xiaojiDriverUninstalledByUser = false; }
    else if (providerId == builtinId(DriverMode::Legacy)) { config_.driverMode = DriverMode::Legacy; config_.ddDriverUninstalledByUser = false; }
    else if (providerId == builtinId(DriverMode::Usb)) config_.driverMode = DriverMode::Usb;
    if (!restartForDriverModeChange()) {
        config_.activeDriverId = original; config_.driverMode = originalMode;
        config_.xiaojiDriverUninstalledByUser = wasXiaojiUninstalled; config_.ddDriverUninstalledByUser = wasDdUninstalled;
        configCoordinator_.save(); loadConfiguredDriver(); refreshDriverStatusUi();
        return DriverCoordinator::SwitchResult::Failed;
    }
    return DriverCoordinator::SwitchResult::Succeeded;
}
