#include "MainWindow.h"

#include "dialogs/AppMessageDialog.h"
#include "dialogs/ConfigConfirmDialog.h"
#include "dialogs/QmlDialog.h"
#include "dialogs/ShareExportDialog.h"
#include "dialogs/ShareImportDialog.h"
#include "FactionTheme.h"
#include "services/AgentService.h"
#include "services/CodeIntegrityInspector.h"
#include "ui/models/ShareImportBannerModel.h"
#include "ui/models/DriverPageModel.h"
#include "ui/models/KeyPageModel.h"
#include "ui/models/SettingsPageModel.h"
#include "ui/models/KeyPageModel.h"
#include "ui/models/MacroPageModel.h"
#include "ui/models/PluginCenterModel.h"
#include "ui/models/SidebarModel.h"
#include "ui/styles/DesignTokens.h"
#include "core/plugins/DeviceDiscovery.h"

#include <QAction>
#include <QFileInfo>
#include <QApplication>
#include <QClipboard>
#include <QCloseEvent>
#include <QDialog>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QEvent>
#include <QFont>
#include <QMimeData>
#include <QProcess>
#include <QScreen>
#include <QScopeGuard>
#include <QString>
#include <QThread>
#include <QTimer>
#include <QtGlobal>
#include <QWindow>
#include <Windows.h>
#include <Dbt.h>
#include <Shellapi.h>
#include <SetupAPI.h>
#include <algorithm>
#include <memory>
#include <stdexcept>
#include <sstream>

namespace {
constexpr int kBaseWindowWidth = 780;
constexpr int kBaseWindowHeight = 560;
constexpr int kBaseDpi = 96;
constexpr wchar_t kXiaoJiUsbVidPid[] = L"vid_1209&pid_584a";
constexpr wchar_t kBundledUsbDriverVersion[] = L"XiaoJi USB V1.1.4";
constexpr GUID kGuidDevInterfaceUsbDevice = {
    0xA5DCBF10,
    0x6530,
    0x11D2,
    {0x90, 0x1F, 0x00, 0xC0, 0x4F, 0xB9, 0x51, 0xED}
};
constexpr GUID kGuidDevInterfaceComPort = {
    0x86E0D1E0,
    0x8089,
    0x11D0,
    {0x9C, 0xE4, 0x08, 0x00, 0x3E, 0x30, 0x1F, 0x73}
};

int dpiScaled(int value, int dpi) {
    return qMax(value, (value * dpi + kBaseDpi / 2) / kBaseDpi);
}

int dpiFromWParam(WPARAM value) {
    const int dpiX = LOWORD(value);
    const int dpiY = HIWORD(value);
    return qMax(kBaseDpi, dpiX > 0 ? dpiX : dpiY);
}

#ifdef Q_OS_WIN
void* registerDeviceInterfaceNotification(HWND hwnd, const GUID& classGuid) {
    if (!hwnd) {
        return nullptr;
    }

    DEV_BROADCAST_DEVICEINTERFACE_W filter{};
    filter.dbcc_size = sizeof(filter);
    filter.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
    filter.dbcc_classguid = classGuid;
    return ::RegisterDeviceNotificationW(hwnd, &filter, DEVICE_NOTIFY_WINDOW_HANDLE);
}

bool isXiaoJiUsbDeviceChange(WPARAM eventType, LPARAM eventData) {
    if (eventType != DBT_DEVICEARRIVAL && eventType != DBT_DEVICEREMOVECOMPLETE) {
        return false;
    }

    const auto* header = reinterpret_cast<const DEV_BROADCAST_HDR*>(eventData);
    if (!header || header->dbch_devicetype != DBT_DEVTYP_DEVICEINTERFACE) {
        return false;
    }

    const auto* deviceInterface = reinterpret_cast<const DEV_BROADCAST_DEVICEINTERFACE_W*>(header);
    const QString devicePath = QString::fromWCharArray(deviceInterface->dbcc_name);
    return devicePath.contains(QString::fromWCharArray(kXiaoJiUsbVidPid), Qt::CaseInsensitive);
}

int effectiveMonitorDpi(HWND hwnd) {
    if (!hwnd) {
        return 0;
    }

    using GetDpiForMonitorFn = HRESULT(WINAPI*)(HMONITOR, int, UINT*, UINT*);
    static const auto getDpiForMonitor = []() -> GetDpiForMonitorFn {
        const HMODULE module = LoadLibraryW(L"Shcore.dll");
        if (!module) {
            return nullptr;
        }
        return reinterpret_cast<GetDpiForMonitorFn>(GetProcAddress(module, "GetDpiForMonitor"));
    }();

    if (!getDpiForMonitor) {
        return 0;
    }

    const HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    if (!monitor) {
        return 0;
    }

    UINT dpiX = 0;
    UINT dpiY = 0;
    constexpr int kEffectiveDpi = 0;
    if (FAILED(getDpiForMonitor(monitor, kEffectiveDpi, &dpiX, &dpiY))) {
        return 0;
    }
    return qMax(kBaseDpi, static_cast<int>(dpiX > 0 ? dpiX : dpiY));
}
#endif

void expandSuggestedRectForDpi(RECT* rect, int dpi) {
    if (!rect) {
        return;
    }

    const int minWidth = dpiScaled(kBaseWindowWidth, qMax(kBaseDpi, dpi));
    const int minHeight = dpiScaled(kBaseWindowHeight, qMax(kBaseDpi, dpi));
    const int width = rect->right - rect->left;
    const int height = rect->bottom - rect->top;
    if (width < minWidth) {
        rect->right = rect->left + minWidth;
    }
    if (height < minHeight) {
        rect->bottom = rect->top + minHeight;
    }
}

void applySuggestedDpiRect(HWND hwnd, const RECT* rect) {
    if (!hwnd || !rect) {
        return;
    }

    const int width = rect->right - rect->left;
    const int height = rect->bottom - rect->top;
    if (width <= 0 || height <= 0) {
        return;
    }

    SetWindowPos(hwnd, nullptr,
        rect->left,
        rect->top,
        width,
        height,
        SWP_NOZORDER | SWP_NOACTIVATE);
}

struct XiaojiInstallResult {
    bool ok = false;
    std::wstring message;
};

class DriverProgressDialog final : public QmlDialog {
public:
    DriverProgressDialog(const QString& title, const QString& message, QWidget* parent)
        : QmlDialog(parent) {
        setWindowTitle(title);
        setWindowModality(Qt::ApplicationModal);
        present("DialogReport.qml", {{"heading", QString()}, {"body", message}, {"bodyRich", false},
            {"status", QString()}, {"severity", 0}, {"progressVisible", true},
            {"progressIndeterminate", true}, {"progressValue", 0}, {"progressText", QString()},
            {"closeAllowed", false}}, QSize(420, 168), true);
    }
    void markFinished(const QString& message) {
        setValue("progressVisible", false);
        setMessage(message);
    }
    void setMessage(const QString& message) { setValue("body", message); }
};

}

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent),
      onboardingCoordinator_(this),
      appPaths_(),
      configCoordinator_(appPaths_.configPath(), this),
      profileController_(config_),
      macroController_(config_),
      engine_(inputDriver_),
      macroEngine_(inputDriver_),
      inputHook_(this),
      rawInput_(this),
      ddRuntimeService_(std::filesystem::path(appPaths_.exeDir()), appPaths_.logsDir()),
      xiaojiRuntimeService_(std::filesystem::path(appPaths_.exeDir())),
      soundService_(appPaths_.exeDir()),
      statusOverlay_(nullptr),
      driverSelfCheckCoordinator_(this),
      updateService_(this),
      updateCoordinator_(this) {
    runtimeVisualRefreshTimer_.setSingleShot(true);
    connect(&runtimeVisualRefreshTimer_, &QTimer::timeout, this, [this]() {
        applyRuntimeVisualRefresh();
    });
    usbDeviceRefreshTimer_.setSingleShot(true);
    connect(&usbDeviceRefreshTimer_, &QTimer::timeout, this, [this]() {
        refreshUsbDriverAfterDeviceChange();
    });
    usbHeartbeatTimer_.setInterval(750);
    connect(&usbHeartbeatTimer_, &QTimer::timeout, this, [this] {
        if (pluginService_) pluginService_->pollActiveDriver();
    });
}

MainWindow::~MainWindow() {
    unregisterUsbDeviceNotifications();
    // 先停前台监视，避免析构期间回调再投递到已销毁的窗口。
    gameWatcher_.SetCallback(nullptr);
    gameWatcher_.SetWindowChangeCallback(nullptr);
    gameWatcher_.Stop();
    inputHook_.stop();
    engineStateCoordinator_.stopThreads();
    inputDriver_.ReleaseAll();
    inputDriver_.ClearAllStates();
    if (pluginService_) pluginService_->shutdownRuntimes();
}

bool MainWindow::Initialize() {
    if (!setupUi()) return false;
    registerUsbDeviceNotifications();
    std::wstring agentError;
    if (!AgentService::Uninstall(&agentError)) {
        ddRuntimeService_.LogRuntimeError(L"dd-runtime.log", L"Legacy Agent cleanup: " + agentError);
    }
    if (!AgentService::RecoverFrontendConfig(appPaths_.configPath())) {
        ddRuntimeService_.LogRuntimeError(
            L"dd-runtime.log", L"Legacy Agent configuration recovery failed; startup canceled.");
        AppMessageDialog::warning(this,
            QString::fromWCharArray(L"\u914d\u7f6e\u6062\u590d\u5931\u8d25"),
            QString::fromWCharArray(
                L"\u65e0\u6cd5\u5c06\u65e7\u540e\u53f0\u914d\u7f6e\u6062\u590d\u5230\u7a0b\u5e8f\u76ee\u5f55\u3002"
                L"\u539f\u914d\u7f6e\u4e0e\u5907\u4efd\u5df2\u4fdd\u7559\uff0c\u672c\u6b21\u542f\u52a8\u5df2\u53d6\u6d88\u3002"
                L"\u8bf7\u68c0\u67e5\u7a0b\u5e8f\u76ee\u5f55\u7684\u5199\u5165\u6743\u9650\u548c\u78c1\u76d8\u7a7a\u95f4\u540e\u91cd\u8bd5\u3002"));
        return false;
    }
    configCoordinator_.load();
    QStringList sidebarOrder;
    for (const auto& id : config_.sidebarPageOrder) sidebarOrder.append(QString::fromStdString(id));
    view_.sidebar()->restorePageOrder(sidebarOrder);
    connect(view_.sidebar(), &SidebarModel::pageOrderChanged, this, [this] {
        config_.sidebarPageOrder.clear();
        for (const auto& id : view_.sidebar()->pageOrder()) config_.sidebarPageOrder.push_back(id.toStdString());
        configCoordinator_.save();
    });
    initializePlugins();
    usageStatisticsCoordinator_.initialize(config_, *view_.usagePage());
    updateCoordinator_.restoreBadgeFromConfig();
    updateCoordinator_.startBackgroundChecks();
    if (config_.backgroundAutoRun) {
        config_.backgroundAutoRun = false;
        configCoordinator_.save(false, 500);
    }
    // 开机自启：启动时对齐注册表 Run 键，修正 exe 移动或注册表被清理后的漂移。
    applyAutoStartSettings();
    // 后台兜底清理更新残留的 .old.exe（启动时的同步清理可能因旧进程
    // 未退出或文件被占用而失败）。
    UpdateService::retryOldExeCleanupInBackground(this);
    soundService_.warmUpSounds(config_.sounds);
    if (!loadConfiguredDriver()) {
        logDriverUnavailableOnce();
        // 驱动加载失败不区分根因，等主窗口首次显示后自动打开自检向导
        // （驻托盘启动则推迟到用户打开主窗口时）；根因由向导内部诊断分诊。
        if (!suppressDdUnavailablePrompt_ && !activeDriverUninstalledByUser()
            && pluginService_ && pluginService_->available(QString::fromStdString(config_.activeDriverId))) {
            pendingSelfCheckPrompt_ = true;
        }
    }
    usbHeartbeatTimer_.start();
    try {
        setupHooks();
    } catch (const std::exception&) {
        AppMessageDialog::warning(this, QString::fromWCharArray(L"\u5168\u5c40\u94a9\u5b50\u521d\u59cb\u5316\u5931\u8d25"),
            QString::fromWCharArray(L"\u65e0\u6cd5\u5b89\u88c5\u5168\u5c40\u952e\u76d8/\u9f20\u6807\u94a9\u5b50"));
        return false;
    }
    engineStateCoordinator_.startThreads();
    engineStateCoordinator_.applyBindings();
    engineStateCoordinator_.applyMacros();
    // 前台门控：监视游戏客户端窗口，按设置里的“生效范围”挂起 / 恢复引擎。
    gameWatcher_.SetCallback([this](bool gameForeground) {
        QMetaObject::invokeMethod(this, [this, gameForeground]() {
            onGameForegroundChanged(gameForeground);
        }, Qt::QueuedConnection);
    });
    gameWatcher_.SetWindowChangeCallback([this](HWND) {
        engine_.InvalidateLongPressForeground();
        QMetaObject::invokeMethod(this, [this]() {
            onForegroundWindowChanged(::GetForegroundWindow());
        }, Qt::QueuedConnection);
    });
    applyGameGateSettings();
    gameWatcher_.Start();
    pageRefreshCoordinator_.refreshAll();
    refreshStatusOverlay();
    bindDpiRefreshSignals();
    observedScreenDpi_ = currentScreenDpi();
    QTimer::singleShot(250, this, [this]() {
        view_.initializeTray(this);
    });
    return true;
}

void MainWindow::ShowForStartup() {
    if (config_.startupAction == StartupAction::Tray) {
        view_.initializeTray(this);
        hide();
        return;
    }
    show();
}

bool MainWindow::loadConfiguredDriver() {
    return pluginService_ && loadPluginDriver();
}

void MainWindow::registerUsbDeviceNotifications() {
#ifdef Q_OS_WIN
    unregisterUsbDeviceNotifications();
    const HWND hwnd = reinterpret_cast<HWND>(winId());
    usbDeviceNotification_ = registerDeviceInterfaceNotification(hwnd, kGuidDevInterfaceUsbDevice);
    usbComPortNotification_ = registerDeviceInterfaceNotification(hwnd, kGuidDevInterfaceComPort);
#endif
}

void MainWindow::unregisterUsbDeviceNotifications() {
#ifdef Q_OS_WIN
    if (usbDeviceNotification_) {
        ::UnregisterDeviceNotification(static_cast<HDEVNOTIFY>(usbDeviceNotification_));
        usbDeviceNotification_ = nullptr;
    }
    if (usbComPortNotification_) {
        ::UnregisterDeviceNotification(static_cast<HDEVNOTIFY>(usbComPortNotification_));
        usbComPortNotification_ = nullptr;
    }
#endif
}

void MainWindow::refreshUsbDriverAfterDeviceChange() {
    if (!pluginService_ || config_.activeDriverId != "xiaoji.driver.usb/default" || exitRequested_ || systemShutdownRequested_
        || driverUninstallInProgress_ || !pluginService_->available(QStringLiteral("xiaoji.driver.usb/default"))) {
        usbHeartbeatReconnectPending_ = false;
        refreshDriverStatusUi();
        return;
    }
    if (usbHeartbeatReconnectPending_ && activeInputDriverReady()) { usbHeartbeatReconnectPending_ = false; return; }
    if (pluginService_->busy() || inputHook_.isDialogOpen()) { usbDeviceRefreshTimer_.start(250); return; }
    usbHeartbeatReconnectPending_ = false;
    engineStateCoordinator_.setRepeatRunning(false);
    macroEngine_.StopAll();
    inputDriver_.ReleaseAll();
    inputDriver_.ClearAllStates();
    loadPluginDriver();
    engineHookCoordinator_.refreshState();
    refreshDriverStatusUi();
    refreshStatusOverlay();
}

bool MainWindow::activeInputDriverReady() const {
    return inputDriver_.IsReady();
}

std::wstring MainWindow::activeInputDriverError() const {
    if (activeInputDriverReady()) return {};
    if (!driverStatusNote_.empty()) return driverStatusNote_;
    if (pluginService_) {
        const auto* driver = pluginService_->activeDriver();
        return driver ? driver->GetLastError() : tr("所选驱动插件尚未连接。").toStdWString();
    }
    return tr("插件服务尚未初始化。").toStdWString();
}

bool MainWindow::connectPluginDriver(Plugins::NativePluginDriver* driver, const QString& providerId, std::wstring* error) {
    if (!driver) return false;
    if (driver->connect()) { if (error) error->clear(); return true; }
    const auto firstError = driver->GetLastError();
    if (providerId != QStringLiteral("xiaoji.driver.dd/default")) { if (error) *error = firstError; return false; }
    if (!ddRuntimeService_.Inspect(false, firstError).legacyRuntimeExists) { if (error) *error = firstError; return false; }

    const bool wasDialogOpen = inputHook_.isDialogOpen();
    const bool wasBusy = view_.driverPage()->state().value("busy").toBool();
    inputHook_.setDialogOpen(true);
    view_.driverPage()->setBusy(true);
    const auto restore = qScopeGuard([this, wasDialogOpen, wasBusy] {
        view_.driverPage()->setBusy(wasBusy);
        inputHook_.setDialogOpen(wasDialogOpen);
    });
    DriverProgressDialog progress(tr("修复 DD 驱动"), tr("正在检查 DD 连接，刷新或重建设备节点，请稍候。"), this);
    struct Result { bool ok = false; std::wstring message; };
    const auto result = std::make_shared<Result>();
    auto worker = std::unique_ptr<QThread>(QThread::create([service = ddRuntimeService_, driver, result] {
        try {
            result->ok = service.RepairEmbeddedInfDriver([driver] { return driver->connect(); }, &result->message);
        } catch (const std::exception& exception) { result->message = ToWide(exception.what()); }
        catch (...) { result->message = L"DD repair failed unexpectedly."; }
    }));
    connect(worker.get(), &QThread::finished, &progress, &QDialog::accept, Qt::QueuedConnection);
    worker->start();
    progress.exec();
    worker->wait();
    if (error) {
        error->clear();
        if (!result->ok) {
            *error = firstError;
            if (!result->message.empty()) *error += L"\n" + result->message;
            const auto lastError = driver->GetLastError();
            if (!lastError.empty() && lastError != firstError) *error += L"\n" + lastError;
        }
    }
    return result->ok;
}

bool MainWindow::ensureLegacyDdInfInstalled(std::wstring* resultMessage) {
    const auto status = ddRuntimeService_.Inspect(activeInputDriverReady(), activeInputDriverError());
    if (status.infKeyboardServiceInstalled && status.infMouseServiceInstalled &&
        status.infKeyboardSysInstalled && status.infMouseSysInstalled) {
        if (resultMessage) {
            *resultMessage = L"\u65e7 DD INF \u9a71\u52a8\u5df2\u5c31\u7eea\u3002";
        }
        return true;
    }

    std::wstring installMessage;
    const bool installed = ddRuntimeService_.InstallEmbeddedInfDriver(&installMessage);
    if (resultMessage) {
        *resultMessage = installMessage;
    }
    if (!installed) {
        return false;
    }

    const auto postInstallStatus = ddRuntimeService_.Inspect(activeInputDriverReady(), activeInputDriverError());
    return postInstallStatus.infKeyboardServiceInstalled &&
        postInstallStatus.infMouseServiceInstalled &&
        postInstallStatus.infKeyboardSysInstalled &&
        postInstallStatus.infMouseSysInstalled;
}

bool MainWindow::prepareXiaojiDriverForSwitch() {
    const bool wasDialogOpen = inputHook_.isDialogOpen();
    inputHook_.setDialogOpen(true);
    view_.driverPage()->setBusy(true);
    const auto restoreUi = qScopeGuard([this, wasDialogOpen] {
        view_.driverPage()->setBusy(false);
        inputHook_.setDialogOpen(wasDialogOpen);
    });
    const auto status = xiaojiRuntimeService_.Inspect();
    const bool needsInstall = !status.keyboardReady || !status.mouseReady || !status.profileReady;
    const bool repairExisting = needsInstall && status.boundPackagePresent;
    const auto infPath = xiaojiRuntimeService_.BundledInfPath();
    const bool signedBundle = needsInstall && !repairExisting
        && QFileInfo(QString::fromStdWString(infPath.wstring())).isFile();
    const bool localInstall = needsInstall && !repairExisting && !signedBundle;
    std::wstring approvedToken;
    if (needsInstall && !status.managerExists) {
        AppMessageDialog::warning(this, tr("管理工具缺失"),
            tr("请从完整发布包恢复 driver/xiaoji/xiaoji.exe 后再安装或修复小叽驱动。"));
        return false;
    }
    if (localInstall) {
        if (!status.installerExists) {
            AppMessageDialog::warning(this, tr("本机签名组件缺失"),
                tr("请从完整发布包恢复 driver/xiaoji/xiaoji-setup.exe。该组件已内置驱动安装包。"));
            return false;
        }
        std::wstring error;
        if (!xiaojiRuntimeService_.ReadTrustToken(&approvedToken, &error)) {
            AppMessageDialog::warning(this, tr("无法读取签名证书状态"), QString::fromStdWString(error));
            return false;
        }
        if (!approvedToken.empty()) {
            const QString fingerprint = approvedToken == L"new" ? tr("首次生成") : QString::fromStdWString(approvedToken);
            const bool approved = ConfigConfirmDialog::confirm(this, tr("信任小叽本机签名证书"),
                tr("将创建或复用小叽的本机签名证书，并将其公钥加入 Windows 的受信任根证书颁发机构和受信任发布者。"
                   "这会让本机信任该发布者。私钥保留在本机，不会上传。\n\n证书指纹：%1\n\n"
                   "确认后自动签名和安装内置驱动，不更改 Secure Boot 或测试签名启动设置。是否继续？").arg(fingerprint), tr("信任并安装"));
            if (!approved) return false;
        }
    }
    engineStateCoordinator_.setRepeatRunning(false);
    macroEngine_.StopAll();
    inputDriver_.ReleaseAll();
    if (config_.driverMode == DriverMode::Xiaoji) {
        inputDriver_.SetDriver(nullptr);
        if (pluginService_) pluginService_->disconnectDrivers();
    }
    const QString message = repairExisting ? tr("正在修复设备并升级按键和鼠标支持，完成后重新启动小叽按键。")
        : localInstall ? tr("正在进行本机签名并安装内置驱动，完成后重新启动小叽按键。")
        : signedBundle ? tr("正在校验并安装已签名驱动，完成后重新启动小叽按键。")
        : tr("正在连接小叽驱动的键盘和鼠标通道。");
    auto* progress = new DriverProgressDialog(tr("准备小叽驱动"), message, this);
    const auto result = std::make_shared<XiaojiInstallResult>();
    QString probeError;
    auto* probe = pluginService_ ? pluginService_->openDriver(QStringLiteral("xiaoji.driver.xiaoji/default"), &probeError) : nullptr;
    if (!probe) {
        progress->deleteLater();
        AppMessageDialog::warning(this, tr("插件无法加载"), probeError);
        return false;
    }
    auto* thread = QThread::create([service = xiaojiRuntimeService_, result, infPath,
        repairExisting, localInstall, signedBundle, approvedToken, needsInstall, probe] {
        const auto release = qScopeGuard([probe] { probe->ReleaseAll(); probe->disconnect(); });
        try {
            if (repairExisting && !service.Repair(&result->message)) return;
            if (localInstall && !service.InstallLocal(approvedToken, &result->message)) return;
            if (signedBundle && !service.Install(infPath, &result->message)) return;
            result->ok = probe->connect();
            if (!result->ok && !needsInstall && service.Inspect().boundPackagePresent) {
                // This is an explicit reconnect/repair request, before any output.
                // Rebuild only the two owned nodes if the old channel is stale.
                const auto firstError = probe->GetLastError();
                probe->disconnect();
                std::wstring repairMessage;
                if (service.Repair(&repairMessage)) result->ok = probe->connect();
                result->message += L"\n" + firstError + L"\n" + repairMessage;
            }
            if (!result->ok) result->message += L"\n" + probe->GetLastError();
        } catch (const std::exception& error) {
            result->message = ToWide(error.what());
        }
    });
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    connect(thread, &QThread::finished, this, [progress] { progress->accept(); }, Qt::QueuedConnection);
    thread->start();
    progress->exec();
    progress->deleteLater();
    ddRuntimeService_.LogRuntimeError(L"xiaoji-runtime.log",
        (result->ok ? std::wstring(L"Driver preparation completed.\n") : std::wstring(L"Driver preparation failed.\n"))
            + result->message);
    if (result->ok) return true;
    AppMessageDialog::warning(this, tr("小叽驱动未就绪"),
        tr("驱动未能就绪，当前选择保持不变。\n\n%1\n\n详细过程已写入 logs/xiaoji-runtime.log。")
            .arg(QString::fromStdWString(result->message)));
    return false;
}

void MainWindow::refreshDriverStatusUi() {
    if (!view_.driverPage()) return;
    if (driverStatusRefreshRunning_) {
        driverStatusRefreshPending_ = true;
        return;
    }
    driverStatusRefreshRunning_ = true;
    view_.driverPage()->setDriverMode(config_.driverMode);

    struct StatusResult {
        DdRuntimeService::RuntimeStatus runtime;
        CodeIntegrityInspector::GuardStatus guards;
        std::wstring detectedUsbPort;
        bool failed = false;
    };
    const auto result = std::make_shared<StatusResult>();
    const bool ready = activeInputDriverReady();
    const auto providerId = config_.activeDriverId;
    const std::wstring lastError = activeInputDriverError();
    const bool usbReady = (config_.activeDriverId == "xiaoji.driver.usb/default" && activeInputDriverReady());
    const auto usbTokens = pluginService_ ? pluginService_->deviceTokensFor(QStringLiteral("xiaoji.driver.usb")) : QStringList{};
    auto* thread = QThread::create([runtimeService = ddRuntimeService_, result, ready, lastError, usbReady, usbTokens]() {
        try {
            result->runtime = runtimeService.Inspect(ready, lastError);
            result->guards = CodeIntegrityInspector::Inspect(false);
            if (!usbReady) result->detectedUsbPort = Plugins::detectSerialPort(usbTokens).toStdWString();
        } catch (const std::exception&) {
            result->failed = true;
        }
    });
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    connect(thread, &QThread::finished, this, [this, result, providerId]() {
        driverStatusRefreshRunning_ = false;
        if (driverStatusRefreshPending_ || config_.activeDriverId != providerId) {
            driverStatusRefreshPending_ = false;
            refreshDriverStatusUi();
            return;
        }
        if (result->failed) {
            if (auto* page = view_.driverPage()) page->setStatusSummary(tr("驱动状态读取失败，请稍后重试"));
            return;
        }
        if (config_.driverMode == DriverMode::Xiaoji && config_.activeDriverId == "xiaoji.driver.xiaoji/default" && activeInputDriverReady()
            && (!result->runtime.xiaojiKeyboardReady || !result->runtime.xiaojiMouseReady
                || !result->runtime.xiaojiProfileReady)) {
            (void)engineStateCoordinator_.setRepeatRunningFast(false);
            macroEngine_.StopAll();
            inputDriver_.ReleaseAll();
            inputDriver_.SetDriver(nullptr);
            if (pluginService_) pluginService_->disconnectDrivers();
            driverStatusNote_ = tr("小叽驱动设备或输入配置已发生变化，请重新连接或安装/修复。").toStdWString();
            engineHookCoordinator_.refreshState();
            refreshStatusOverlay();
        }
        applyDriverStatusUi(result->runtime, result->guards, result->detectedUsbPort);
    }, Qt::QueuedConnection);
    thread->start();
}

void MainWindow::applyDriverStatusUi(const DdRuntimeService::RuntimeStatus& status,
    const CodeIntegrityInspector::GuardStatus& guards, const std::wstring& detectedUsbPort) {
    auto* page = view_.driverPage();
    if (!page) return;
    page->setDriverMode(config_.driverMode);
    const bool xiaoji = config_.activeDriverId == "xiaoji.driver.xiaoji/default";
    const bool dd = config_.activeDriverId == "xiaoji.driver.dd/default";
    const bool ready = activeInputDriverReady();
    const bool uninstalled = activeDriverUninstalledByUser();
    const bool xiaojiInstalled = status.xiaojiKeyboardPresent || status.xiaojiMousePresent
        || status.xiaojiConfigurationPresent || status.xiaojiBoundPackagePresent;
    const bool ddInstalled = status.infKeyboardServiceInstalled || status.infMouseServiceInstalled
        || status.infKeyboardSysInstalled || status.infMouseSysInstalled || status.infKeyboardDeviceStarted
        || status.infMouseDeviceStarted || status.infKeyboardDeviceBound || status.infMouseDeviceBound;
    const bool oldHidInstalled = status.hidDevicesPresent || status.hidServiceInstalled
        || status.hidServicePendingDelete || status.hidSysInstalled || status.hidDriverStorePresent;
    const auto providerId = QString::fromStdString(config_.activeDriverId);
    const bool providerAvailable = pluginService_ && pluginService_->available(providerId);
    page->setDriverActions(providerAvailable && xiaoji && xiaojiInstalled, providerAvailable && dd && ddInstalled, oldHidInstalled);
    const auto ui = pluginService_ ? pluginService_->driverUi(providerId) : QVariantMap{};
    page->setPluginManagement(ui.value("repair").toBool(), ui.value("uninstall").toBool()
        && (xiaoji ? xiaojiInstalled : dd ? ddInstalled : true));
    page->setDiagnosticsActions(ui.value("diagnostics").toBool(), ui.value("selfCheck").toBool());
    const auto* pluginDriver = pluginService_ ? pluginService_->activeDriver() : nullptr;
    const auto pluginDetails = pluginDriver ? pluginDriver->text(XJ_TEXT_DIAGNOSTIC) : QString();
    const QString name = pluginService_ ? pluginService_->driverName(QString::fromStdString(config_.activeDriverId)) : QString();
    if (!xiaoji && !dd && config_.activeDriverId != "xiaoji.driver.usb/default") {
        page->setStatusSummary(ready ? tr("%1 · 已就绪").arg(name) : tr("未就绪"));
        page->setInstallAction(!config_.activeDriverId.empty() && !ready, tr("重新连接"));
        page->setDetailText(tr("驱动提供方：%1\n状态：%2\n%3")
            .arg(name.isEmpty() ? tr("尚未选择驱动") : name, ready ? tr("已就绪") : tr("未连接"),
                QString::fromStdWString(activeInputDriverError()) + QLatin1Char('\n') + pluginDetails));
        return;
    }
    page->setStatusSummary(driverUninstallFailed_ ? tr("卸载未完成 · 可重试")
        : uninstalled ? (xiaoji ? (xiaojiInstalled ? tr("已停用 · 仍有安装残留") : tr("已卸载"))
            : tr("已停用 · 主动卸载")) : ready ? tr("可用")
        : dd && !status.legacyRuntimeExists ? tr("不可用 · 运行文件缺失") : tr("未就绪 · 请连接或查看诊断"));
    page->setInstallAction(!ready && ((xiaoji && (status.xiaojiManagerExists
            || (status.xiaojiKeyboardReady && status.xiaojiMouseReady && status.xiaojiProfileReady))) || (dd && status.legacyRuntimeExists)),
        xiaoji && status.xiaojiKeyboardReady && status.xiaojiMouseReady && status.xiaojiProfileReady ? tr("重新连接小叽")
            : uninstalled ? tr("安装 %1").arg(name) : tr("安装/修复 %1").arg(name));
    QStringList lines;
    lines << tr("配置驱动: %1").arg(name);
    if (!activeInputDriverError().empty()) lines << tr("当前错误: %1").arg(QString::fromStdWString(activeInputDriverError()));
    if (xiaoji && providerAvailable) {
        lines << QString() << name
            << tr("键盘: %1").arg(status.xiaojiKeyboardReady ? tr("已就绪") : status.xiaojiKeyboardPresent ? tr("未启动或身份异常") : tr("未安装"))
            << tr("鼠标: %1").arg(status.xiaojiMouseReady ? tr("已就绪") : status.xiaojiMousePresent ? tr("未启动或身份异常") : tr("未安装"))
            << tr("管理工具: %1").arg(status.xiaojiManagerExists ? tr("已找到") : tr("未找到"))
            << tr("本机签名安装: %1").arg(status.xiaojiInstallerExists ? tr("可用") : tr("组件缺失"))
            << tr("输入支持: %1").arg(status.xiaojiProfileReady ? tr("已启用") : tr("需安装或升级配置"));
        if (!status.xiaojiError.empty()) lines << QString::fromStdWString(status.xiaojiError);
    }
    if (dd && providerAvailable) {
        lines << QString() << name
            << tr("运行文件: %1").arg(status.legacyRuntimeExists ? tr("已找到") : tr("未找到"))
            << tr("键盘服务/设备: %1 / %2").arg(status.infKeyboardServiceInstalled ? tr("已安装") : tr("未安装"),
                status.infKeyboardDeviceStarted && status.infKeyboardDeviceBound ? tr("已就绪") : tr("未就绪"))
            << tr("鼠标服务/设备: %1 / %2").arg(status.infMouseServiceInstalled ? tr("已安装") : tr("未安装"),
                status.infMouseDeviceStarted && status.infMouseDeviceBound ? tr("已就绪") : tr("未就绪"));
    }
    if (config_.activeDriverId == "xiaoji.driver.usb/default" && providerAvailable) {
        const bool usbReady = activeInputDriverReady();
        const auto usbPort = usbReady && pluginDriver ? pluginDriver->text(XJ_TEXT_PORT).toStdWString() : detectedUsbPort;
        lines << QString() << name
            << tr("设备: %1").arg(usbPort.empty() ? tr("未插入") : tr("已插入"))
            << tr("通信: %1").arg(usbReady ? tr("已连接") : tr("未连接"));
        if (usbReady && pluginDriver) lines << tr("固件: %1").arg(pluginDriver->text(XJ_TEXT_FIRMWARE));
    }
    if (!pluginDetails.isEmpty()) lines << QString() << pluginDetails;
    if (oldHidInstalled) lines << QString() << tr("旧 HID: 检测到安装残留，仅提供卸载入口")
        << tr("服务: %1；设备: %2；驱动包: %3")
            .arg(status.hidServicePendingDelete ? tr("等待重启删除") : status.hidServiceInstalled ? tr("存在") : tr("无"),
                status.hidDevicesPresent ? tr("存在") : tr("无"), status.hidDriverStorePresent ? tr("存在") : tr("无"));
    lines << QString() << tr("内存完整性: %1（仅供核对）")
        .arg(guards.memoryIntegrityRunning ? tr("运行中") : guards.memoryIntegrityConfigured ? tr("等待重启生效") : tr("关闭"));
    page->setDetailText(lines.join(QLatin1Char('\n')));
}

DriverCoordinator::SwitchResult MainWindow::switchDriverMode(DriverMode mode) {
    return switchPluginDriver(mode == DriverMode::Legacy ? QStringLiteral("xiaoji.driver.dd/default")
        : mode == DriverMode::Usb ? QStringLiteral("xiaoji.driver.usb/default") : QStringLiteral("xiaoji.driver.xiaoji/default"));
}

bool MainWindow::restartForDriverModeChange() {
    if (!configCoordinator_.save()) return false;
    suppressDdUnavailablePrompt_ = true;

    const QString program = QCoreApplication::applicationFilePath();
    const QStringList arguments = { QStringLiteral("--restart-wait") };
    if (!QProcess::startDetached(program, arguments, QCoreApplication::applicationDirPath())) {
        suppressDdUnavailablePrompt_ = false;
        driverStatusNote_ = L"\u5207\u6362\u9a71\u52a8\u9700\u8981\u91cd\u542f\uff0c\u4f46\u65b0\u8fdb\u7a0b\u542f\u52a8\u5931\u8d25\u3002";
        return false;
    }

    quitApplication();
    return true;
}

bool MainWindow::prepareDriverUninstall(DriverRemovalTarget target) {
    if (driverUninstallInProgress_) return false;
    driverUninstallInProgress_ = true;
    // A deliberate disconnect is not a driver failure. Invalidate already
    // queued automatic checks before stopping input or opening nested dialogs.
    ++selfCheckPromptGeneration_;
    pendingSelfCheckPrompt_ = false;
    if (target == DriverRemovalTarget::RetiredHid) return true;
    const auto mode = target == DriverRemovalTarget::Xiaoji ? DriverMode::Xiaoji : DriverMode::Legacy;
    driverUninstallPreviousState_ = config_.IsDriverUninstalledByUser(mode);
    config_.SetDriverUninstalledByUser(mode, true);
    if (!configCoordinator_.save()) {
        config_.SetDriverUninstalledByUser(mode, driverUninstallPreviousState_);
        driverUninstallInProgress_ = false;
        return false;
    }
    const auto targetId = mode == DriverMode::Xiaoji ? "xiaoji.driver.xiaoji/default" : "xiaoji.driver.dd/default";
    if (config_.activeDriverId != targetId) return true;
    selfCheckAutoOpened_ = true;
    engineStateCoordinator_.setRepeatRunning(false);
    macroEngine_.StopAll();
    inputDriver_.ReleaseAll();
    inputDriver_.ClearAllStates();
    inputDriver_.SetDriver(nullptr);
    if (pluginService_) pluginService_->disconnectDrivers();
    return true;
}

void MainWindow::finalizeDriverUninstall(DriverRemovalTarget target, bool succeeded) {
    driverUninstallInProgress_ = false;
    ++selfCheckPromptGeneration_;
    if (target != DriverRemovalTarget::RetiredHid) {
        const auto mode = target == DriverRemovalTarget::Xiaoji ? DriverMode::Xiaoji : DriverMode::Legacy;
        config_.SetDriverUninstalledByUser(mode, succeeded || driverUninstallPreviousState_);
        if (!succeeded) configCoordinator_.save();
    }
    const bool active = (target == DriverRemovalTarget::Xiaoji && config_.activeDriverId == "xiaoji.driver.xiaoji/default")
        || (target == DriverRemovalTarget::Legacy && config_.activeDriverId == "xiaoji.driver.dd/default");
    if (active) {
        inputDriver_.SetDriver(nullptr);
        driverUninstallFailed_ = !succeeded;
        driverStatusNote_ = succeeded
            ? tr("当前驱动已卸载，需要再次使用时请在驱动管理页主动安装。").toStdWString()
            : tr("卸载未完成，当前输入已断开。请通过保留的卸载按钮重试；需要恢复使用时可主动重新连接或安装/修复。").toStdWString();
    }
    engineHookCoordinator_.refreshState();
    refreshDriverStatusUi();
}

bool MainWindow::rebootComputer() {
    const QString program = QStringLiteral("shutdown.exe");
    const QStringList arguments = {
        QStringLiteral("/r"),
        QStringLiteral("/t"), QStringLiteral("0"),
        QStringLiteral("/f")
    };
    return QProcess::startDetached(program, arguments, QCoreApplication::applicationDirPath());
}

void MainWindow::applyStagedUpdateAndRestart(const QString& stagingDir) {
    if (!configCoordinator_.save()) return;

    // Stop output before replacement. Loaded native modules stay resident;
    // locked DLLs are promoted from .update-pending by the next process.
    (void)engineStateCoordinator_.setRepeatRunningFast(false);
    macroEngine_.StopAll();
    inputDriver_.ReleaseAll();
    inputDriver_.ClearAllStates();
    inputDriver_.SetDriver(nullptr);
    if (pluginService_) pluginService_->disconnectDrivers();
    // 常驻 MCI 音效设备占着 audio/ 内置 wav 的句柄，不先释放会锁住更新覆盖。
    soundService_.closeAllAliases();

    QString error;
    const bool updated = UpdateService::applyStagedUpdate(stagingDir, &error);
    soundService_.resumePlayback();
    if (!updated) {
        // Best effort: bring the driver back so the application stays usable.
        if (!loadConfiguredDriver()) {
            logDriverUnavailableOnce();
        }
        engineStateCoordinator_.updateRepeatStatus();
        refreshStatusOverlay();
        refreshDriverStatusUi();
        AppMessageDialog::warning(
            this,
            QString::fromWCharArray(L"\u66f4\u65b0\u5931\u8d25"),
            error + QString::fromWCharArray(
                L"\u3002\u4e5f\u53ef\u524d\u5f80\u5f00\u6e90\u4ed3\u5e93\u624b\u52a8\u4e0b\u8f7d\u6700\u65b0\u7248\u672c\u3002"));
        return;
    }

    suppressDdUnavailablePrompt_ = true;
    // 此刻原 exe 已被改名为 .old.exe，必须显式拉起新 exe；
    // 不能用 applicationFilePath()（若未缓存会解析成改名后的旧程序，
    // 旧程序会一直锁住 .old.exe 导致清理永远失败）。
    const QString program = QCoreApplication::applicationDirPath()
        + QStringLiteral("/XiaoJiAutoKey.exe");
    const QStringList arguments = { QStringLiteral("--restart-wait") };
    if (!QProcess::startDetached(program, arguments, QCoreApplication::applicationDirPath())) {
        AppMessageDialog::warning(
            this,
            QString::fromWCharArray(L"\u66f4\u65b0\u5b8c\u6210"),
            QString::fromWCharArray(
                L"\u65b0\u7248\u672c\u5df2\u5c31\u7eea\uff0c\u4f46\u65e0\u6cd5\u81ea\u52a8\u91cd\u542f\uff0c\u8bf7\u624b\u52a8\u6253\u5f00\u5c0f\u53fd\u6309\u952e\u3002"));
    }
    quitApplication();
}

bool MainWindow::restartApplication(const QString& reasonMessage) {
    if (!configCoordinator_.save()) return false;
    suppressDdUnavailablePrompt_ = true;

    AppMessageDialog::information(
        this,
        QString::fromWCharArray(L"\u7a0b\u5e8f\u91cd\u542f"),
        reasonMessage);

    const QString program = QCoreApplication::applicationFilePath();
    const QStringList arguments = { QStringLiteral("--restart-wait") };
    if (!QProcess::startDetached(program, arguments, QCoreApplication::applicationDirPath())) {
        suppressDdUnavailablePrompt_ = false;
        return false;
    }

    quitApplication();
    return true;
}

void MainWindow::quitApplication() {
    exitRequested_ = true;
    view_.hideTrayIcon();
    QCoreApplication::quit();
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event) {
    if (event->type() == QEvent::MouseButtonPress && captureCoordinator_.captureActive()) {
        captureCoordinator_.cancelCapture();
    }
    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::showEvent(QShowEvent* event) {
    QMainWindow::showEvent(event);
    if (onboardingStartupScheduled_) return;
    onboardingStartupScheduled_ = true;
    if (!pendingSelfCheckPrompt_) {
        QTimer::singleShot(0, &onboardingCoordinator_, &OnboardingCoordinator::initialize);
        return;
    }
    pendingSelfCheckPrompt_ = false;
    selfCheckAutoOpened_ = true;
    // 先让主窗口完成首帧渲染，再弹自检向导，避免开屏即模态。
    const auto promptGeneration = selfCheckPromptGeneration_;
    QTimer::singleShot(250, this, [this, promptGeneration]() {
        if (promptGeneration != selfCheckPromptGeneration_ || driverUninstallInProgress_
            || activeDriverUninstalledByUser()) {
            onboardingCoordinator_.initialize();
            return;
        }
        connect(&driverSelfCheckCoordinator_, &DriverSelfCheckCoordinator::dialogsClosed,
            &onboardingCoordinator_, &OnboardingCoordinator::initialize, Qt::SingleShotConnection);
        driverSelfCheckCoordinator_.open();
    });
}

bool MainWindow::event(QEvent* event) {
    switch (event->type()) {
    case QEvent::ScreenChangeInternal:
        refreshIfScreenDpiChanged();
        break;
    case QEvent::ActivationChange:
        if (isActiveWindow()) {
            checkClipboardForShareText();
        }
        break;
    default:
        break;
    }
    return QMainWindow::event(event);
}

// 拖入的文本只要是分享内容就接受，直接进入导入预览。
bool MainWindow::acceptShareTextDrag(const QMimeData* mimeData) {
    return mimeData != nullptr && mimeData->hasText()
        && ShareCodec::LooksLikeShareText(mimeData->text().trimmed().toStdString());
}

void MainWindow::dragEnterEvent(QDragEnterEvent* event) {
    if (acceptShareTextDrag(event->mimeData())) {
        event->acceptProposedAction();
        return;
    }
    QMainWindow::dragEnterEvent(event);
}

void MainWindow::dragMoveEvent(QDragMoveEvent* event) {
    if (acceptShareTextDrag(event->mimeData())) {
        event->acceptProposedAction();
        return;
    }
    QMainWindow::dragMoveEvent(event);
}

void MainWindow::dropEvent(QDropEvent* event) {
    if (acceptShareTextDrag(event->mimeData())) {
        event->acceptProposedAction();
        openImportDialog(event->mimeData()->text());
        return;
    }
    QMainWindow::dropEvent(event);
}

bool MainWindow::nativeEvent(const QByteArray& eventType, void* message, qintptr* result) {
    Q_UNUSED(eventType)
#ifdef Q_OS_WIN
    auto* msg = static_cast<MSG*>(message);
    if (msg) {
        if (msg->message == WM_INPUT || msg->message == WM_INPUT_DEVICE_CHANGE) {
            rawInput_.handleNativeMessage(msg, result);
        }
        switch (msg->message) {
        case WM_POWERBROADCAST:
            if (msg->wParam == PBT_APMSUSPEND) {
                usageStatisticsCoordinator_.setSuspended(true);
            } else if (msg->wParam == PBT_APMRESUMEAUTOMATIC || msg->wParam == PBT_APMRESUMESUSPEND) {
                usageStatisticsCoordinator_.setSuspended(false);
            }
            break;
        case WM_QUERYENDSESSION:
            systemShutdownRequested_ = true;
            prepareForSystemShutdown();
            if (result) *result = TRUE;
            return true;
        case WM_ENDSESSION:
            if (msg->wParam != FALSE) {
                systemShutdownRequested_ = true;
                prepareForSystemShutdown();
                QApplication::quit();
            } else {
                systemShutdownRequested_ = false;
                systemShutdownSaveAttempted_ = false;
            }
            if (result) *result = 0;
            return true;
        case WM_DEVICECHANGE:
            if (pluginService_) pluginService_->notifyDeviceChange();
            if (isXiaoJiUsbDeviceChange(msg->wParam, msg->lParam)) {
                usbHeartbeatReconnectPending_ = false;
                usbDeviceRefreshTimer_.start(650);
            } else if (msg->wParam == DBT_DEVNODES_CHANGED && config_.driverMode == DriverMode::Xiaoji) {
                refreshDriverStatusUi();
            }
            break;
        case WM_THEMECHANGED:
            Fluent::Theme::refreshDarkModeCache();
            scheduleRuntimeVisualRefresh(false, true);
            break;
        case WM_SETTINGCHANGE: {
            if (msg->wParam == SPI_SETCLIENTAREAANIMATION)
                scheduleRuntimeVisualRefresh(false, true);
            const auto* area = reinterpret_cast<const wchar_t*>(msg->lParam);
            const QString setting = area ? QString::fromWCharArray(area) : QString();
            const bool themeSetting =
                setting.compare(QStringLiteral("ImmersiveColorSet"), Qt::CaseInsensitive) == 0 ||
                setting.compare(QStringLiteral("WindowsThemeElement"), Qt::CaseInsensitive) == 0;
            const bool dpiSetting =
                setting.isEmpty() ||
                setting.compare(QStringLiteral("Display"), Qt::CaseInsensitive) == 0 ||
                setting.compare(QStringLiteral("UserPreferences"), Qt::CaseInsensitive) == 0 ||
                setting.compare(QStringLiteral("WindowMetrics"), Qt::CaseInsensitive) == 0 ||
                setting.compare(QStringLiteral("ConvertibleSlateMode"), Qt::CaseInsensitive) == 0;
            if (dpiSetting) {
                const int dpi = currentScreenDpi();
                if (observedScreenDpi_ == 0 || qAbs(dpi - observedScreenDpi_) > 2) {
                    runtimeDpiOverride_ = dpi;
                }
                runtimeDpiSyncWaitsRemaining_ = 0;
                scheduleRuntimeVisualRefresh(true, false);
            }
            if (themeSetting) {
                Fluent::Theme::refreshDarkModeCache();
                scheduleRuntimeVisualRefresh(false, true);
            }
            break;
        }
        case WM_DPICHANGED:
            runtimeDpiOverride_ = dpiFromWParam(msg->wParam);
            if (auto* suggestedRect = reinterpret_cast<RECT*>(msg->lParam)) {
                expandSuggestedRectForDpi(suggestedRect, runtimeDpiOverride_);
                applySuggestedDpiRect(reinterpret_cast<HWND>(winId()), suggestedRect);
            }
            runtimeDpiSyncWaitsRemaining_ = 0;
            enforceNativeWindowSizeForDpi(runtimeDpiOverride_);
            scheduleRuntimeVisualRefresh(true, false);
            for (const int delayMs : {0, 120, 320}) {
                const int dpi = runtimeDpiOverride_;
                QTimer::singleShot(delayMs, this, [this, dpi]() {
                    runtimeDpiOverride_ = dpi;
                    runtimeDpiSyncWaitsRemaining_ = 0;
                    scheduleRuntimeVisualRefresh(true, false);
                });
            }
            break;
        case WM_DISPLAYCHANGE:
            refreshIfScreenDpiChanged();
            break;
        default:
            break;
        }
    }
#else
    Q_UNUSED(message)
#endif
    return QMainWindow::nativeEvent(eventType, message, result);
}

void MainWindow::prepareForSystemShutdown() {
    if (systemShutdownSaveAttempted_) return;
    systemShutdownSaveAttempted_ = true;
    configCoordinator_.save(false, 500);
}

void MainWindow::scheduleRuntimeVisualRefresh(bool dpiChanged, bool themeChanged) {
    runtimeDpiRefreshPending_ = runtimeDpiRefreshPending_ || dpiChanged;
    runtimeThemeRefreshPending_ = runtimeThemeRefreshPending_ || themeChanged;
    runtimeVisualRefreshPassesRemaining_ = qMax(runtimeVisualRefreshPassesRemaining_, dpiChanged ? 6 : 2);
    if (applyingRuntimeVisualRefresh_) {
        return;
    }
    runtimeVisualRefreshTimer_.start(dpiChanged ? 0 : 80);
}

void MainWindow::applyRuntimeVisualRefresh() {
    if (applyingRuntimeVisualRefresh_) {
        return;
    }

    applyingRuntimeVisualRefresh_ = true;
    if (runtimeDpiRefreshPending_) {
        runtimeDpiRefreshActive_ = true;
    }
    const bool dpiChanged = runtimeDpiRefreshActive_;
    const bool themeChanged = runtimeThemeRefreshPending_;
    runtimeDpiRefreshPending_ = false;
    runtimeThemeRefreshPending_ = false;

    if (runtimeVisualRefreshPassesRemaining_ > 0) {
        --runtimeVisualRefreshPassesRemaining_;
    }

    QFont globalFont = Fluent::Typography::body();
    qApp->setFont(globalFont);

    qApp->setPalette(Fluent::Theme::applicationPalette());

    const int screenDpi = currentScreenDpi();
    const int dpi = runtimeDpiOverride_ > 0 ? runtimeDpiOverride_ : screenDpi;
    if (dpiChanged) {
        recreateWindowSurfaceAfterDpiChange(dpi);
        observedScreenDpi_ = dpi;
        enforceNativeWindowSizeForDpi(dpi);
    }
    view_.refreshRuntimeVisuals(this, dpiChanged, themeChanged, dpiChanged ? dpi : 0);
    if (dpiChanged) {
        statusOverlay_.refreshForDpiChange(dpi);
    }
    if (themeChanged) {
        pageRefreshCoordinator_.refreshAll();
        view_.refreshRuntimeVisuals(this, false, false);
    }
    refreshStatusOverlay();

    applyingRuntimeVisualRefresh_ = false;
    if (runtimeVisualRefreshPassesRemaining_ > 0) {
        runtimeVisualRefreshTimer_.start(dpiChanged ? 160 : 220);
    } else if (runtimeDpiRefreshActive_) {
        runtimeDpiRefreshActive_ = false;
        runtimeDpiSyncWaitsRemaining_ = 0;
        runtimeDpiOverride_ = 0;
        QTimer::singleShot(0, this, [this]() {
            view_.refreshRuntimeVisuals(this, false, false);
        });
    }
}

void MainWindow::bindDpiRefreshSignals() {
    QTimer::singleShot(0, this, [this]() {
        auto* handle = windowHandle();
        if (!handle) {
            return;
        }

        auto bindScreen = [this](QScreen* screen) {
            if (!screen) {
                return;
            }
            connect(screen, &QScreen::logicalDotsPerInchChanged, this, [this](qreal dpi) {
                runtimeDpiOverride_ = qMax(qMax(kBaseDpi, qRound(dpi)), currentScreenDpi());
                runtimeDpiSyncWaitsRemaining_ = 0;
                scheduleRuntimeVisualRefresh(true, false);
            }, Qt::UniqueConnection);
            connect(screen, &QScreen::physicalDotsPerInchChanged, this, [this](qreal dpi) {
                Q_UNUSED(dpi)
                runtimeDpiOverride_ = currentScreenDpi();
                runtimeDpiSyncWaitsRemaining_ = 0;
                scheduleRuntimeVisualRefresh(true, false);
            }, Qt::UniqueConnection);
        };

        bindScreen(handle->screen());
        connect(handle, &QWindow::screenChanged, this, [this, bindScreen](QScreen* screen) {
            bindScreen(screen);
            refreshIfScreenDpiChanged();
        }, Qt::UniqueConnection);
    });
}

int MainWindow::currentScreenDpi() const {
    QScreen* screen = nullptr;
    if (auto* handle = windowHandle()) {
        screen = handle->screen();
    }
    if (!screen) {
        screen = QApplication::primaryScreen();
    }
    int qtDpi = screen ? qMax(kBaseDpi, qRound(screen->logicalDotsPerInch())) : kBaseDpi;
    if (screen) {
        qtDpi = qMax(qtDpi, qRound(screen->devicePixelRatio() * kBaseDpi));
    }
    if (auto* handle = windowHandle()) {
        qtDpi = qMax(qtDpi, qRound(handle->devicePixelRatio() * kBaseDpi));
    }
    qtDpi = qMax(qtDpi, qRound(devicePixelRatioF() * kBaseDpi));

#ifdef Q_OS_WIN
    const HWND hwnd = reinterpret_cast<HWND>(winId());
    if (hwnd) {
        const int monitorDpi = effectiveMonitorDpi(hwnd);
        if (monitorDpi > 0 && observedScreenDpi_ > 0 && qAbs(monitorDpi - observedScreenDpi_) > 2) {
            return monitorDpi;
        }

        const UINT dpi = GetDpiForWindow(hwnd);
        if (dpi > 0) {
            const int nativeDpi = qMax(kBaseDpi, static_cast<int>(dpi));
            if (observedScreenDpi_ > 0 && qAbs(qtDpi - observedScreenDpi_) > 2) {
                return qtDpi;
            }
            return nativeDpi;
        }
    }
#endif

    return qtDpi;
}

void MainWindow::refreshIfScreenDpiChanged() {
    const int dpi = currentScreenDpi();
    if (observedScreenDpi_ == 0) {
        observedScreenDpi_ = dpi;
        return;
    }
    if (qAbs(dpi - observedScreenDpi_) < 6) {
        return;
    }
    observedScreenDpi_ = dpi;
    scheduleRuntimeVisualRefresh(true, false);
}

void MainWindow::enforceNativeWindowSizeForDpi(int dpi) {
    if (dpi <= 0 || isMinimized()) {
        return;
    }

    const HWND hwnd = reinterpret_cast<HWND>(winId());
    RECT rect{};
    if (!GetWindowRect(hwnd, &rect)) {
        return;
    }

    const int minWidth = dpiScaled(kBaseWindowWidth, qMax(kBaseDpi, dpi));
    const int minHeight = dpiScaled(kBaseWindowHeight, qMax(kBaseDpi, dpi));
    const int width = rect.right - rect.left;
    const int height = rect.bottom - rect.top;
    if (width >= minWidth && height >= minHeight) {
        return;
    }

    SetWindowPos(hwnd, nullptr,
        rect.left,
        rect.top,
        qMax(width, minWidth),
        qMax(height, minHeight),
        SWP_NOZORDER | SWP_NOACTIVATE);
}

void MainWindow::recreateWindowSurfaceAfterDpiChange(int dpi) {
    if (dpi <= kBaseDpi || dpi % kBaseDpi == 0 || isMinimized()) {
        return;
    }

    if (recreatedFractionalDpiSurface_) {
        return;
    }
    recreatedFractionalDpiSurface_ = true;

    const QRect originalGeometry = geometry();
    if (originalGeometry.isEmpty()) {
        return;
    }

    QTimer::singleShot(0, this, [this, originalGeometry, dpi]() {
        if (isMinimized()) {
            return;
        }

        const bool wasVisible = isVisible();
        if (wasVisible) {
            hide();
        }

        destroy(true, true);
        setGeometry(originalGeometry);
        createWinId();
        rawInput_.registerWindow(reinterpret_cast<HWND>(winId()));
        registerUsbDeviceNotifications();
        engineHookCoordinator_.refreshState();

        if (wasVisible) {
            show();
        }

        enforceNativeWindowSizeForDpi(dpi);
        view_.refreshRuntimeVisuals(this, true, false, dpi);
        if (const HWND hwnd = reinterpret_cast<HWND>(winId())) {
            RedrawWindow(hwnd, nullptr, nullptr,
                RDW_INVALIDATE | RDW_FRAME | RDW_ALLCHILDREN | RDW_UPDATENOW);
        }
        QTimer::singleShot(120, this, [this, dpi]() {
            view_.refreshRuntimeVisuals(this, true, false, dpi);
            if (const HWND hwnd = reinterpret_cast<HWND>(winId())) {
                RedrawWindow(hwnd, nullptr, nullptr,
                    RDW_INVALIDATE | RDW_FRAME | RDW_ALLCHILDREN | RDW_UPDATENOW);
            }
        });
    });
}

void MainWindow::syncQtGeometryWithNativeWindow() {
    if (isMinimized()) {
        return;
    }

    const HWND hwnd = reinterpret_cast<HWND>(winId());
    RECT rect{};
    if (!hwnd || !GetWindowRect(hwnd, &rect)) {
        return;
    }

    const qreal scale = devicePixelRatioF();
    if (scale <= 0.0 || qFuzzyCompare(scale, 1.0)) {
        return;
    }

    const QRect logicalGeometry(
        qRound(rect.left / scale),
        qRound(rect.top / scale),
        qRound((rect.right - rect.left) / scale),
        qRound((rect.bottom - rect.top) / scale));
    if (!logicalGeometry.isEmpty() && geometry() != logicalGeometry) {
        setGeometry(logicalGeometry);
    }
}

void MainWindow::refreshStatusOverlay() {
    if (pluginService_) pluginService_->setRuntimeState({{"version", QStringLiteral(APP_VERSION)},
        {"driver", pluginService_->driverName(QString::fromStdString(config_.activeDriverId))},
        {"repeatRunning", engine_.IsToolRunning()}, {"macroRunning", macroEngine_.HasActiveMacro()}});
    auto* profile = profileController_.activeProfile();
    auto* macro = macroController_.activeMacro();
    const auto repeatStatus = engine_.GetStatus();
    const auto macroStatus = macro ? macroEngine_.GetStatus(macro->id) : MacroStatus{};
    const bool repeatRunning = repeatStatus.running;
    const bool macroRunning = macroStatus.running;
    const bool gameGatePaused = config_.gameGateEnabled && !gameWatcher_.IsGameForeground();
    const bool repeatActive = repeatRunning && (!repeatStatus.paused || !repeatStatus.holdKeys.empty());
    const bool macroActive = macroRunning && !macroStatus.paused;
    usageStatisticsCoordinator_.updateRunning(!gameGatePaused
        && (repeatActive || macroActive));
    statusOverlay_.setGameGatePaused(gameGatePaused && (repeatRunning || macroRunning));
    if (config_.statusOverlayShowRepeat || config_.statusOverlayShowMacro) {
        statusOverlay_.updateStatus(config_, profile, macro,
            config_.statusOverlayShowRepeat ? repeatStatus : RepeaterStatus{},
            config_.statusOverlayShowMacro ? macroStatus : MacroStatus{});
    } else {
        statusOverlay_.setOverlayEnabled(false);
    }
    if (auto* keyPage = view_.keyPage()) {
        keyPage->setSequenceSelection(repeatStatus.sequenceTaskId);
        keyPage->setOverviewRuntime(repeatStatus, gameGatePaused);
    }
    view_.macroPage()->setOverviewRuntime(macroStatus.paused || gameGatePaused, macroStatus.currentStep);
    view_.updateActivityIcons(this, repeatRunning, macroRunning);
}

void MainWindow::applyGameGateSettings() {
    ForegroundWatcher::Rules rules;
    rules.enabled = config_.gameGateEnabled;
    rules.customProcesses = config_.gameGateProcesses;
    gameWatcher_.SetRules(std::move(rules));

    if (auto* page = view_.settingsPage()) {
        page->setGameGateEnabled(config_.gameGateEnabled);
    }
    onForegroundWindowChanged(::GetForegroundWindow());
    onGameForegroundChanged(gameWatcher_.IsGameForeground());
}

// 开机自启：把注册表 Run 键与配置对齐（开启时写入当前 exe 路径，关闭时删除残留项）。
void MainWindow::applyAutoStartSettings() {
    AutoStartService::setEnabled(config_.autoStartEnabled);
}

void MainWindow::onGameForegroundChanged(bool gameForeground) {
    const bool suspended = config_.gameGateEnabled && !gameForeground;
    engine_.SetSuspended(suspended);
    macroEngine_.SetSuspended(suspended);
    refreshStatusOverlay();
}

void MainWindow::onForegroundWindowChanged(HWND foregroundWindow) {
    if (foregroundWindow != ::GetForegroundWindow()) return;

    bool allowed = foregroundWindow != nullptr;
    if (allowed && config_.gameGateEnabled) {
        const auto processName = QueryForegroundProcessName(foregroundWindow);
        allowed = ForegroundWatcher::MatchesBuiltInProcessName(processName)
            || ForegroundWatcher::MatchesCustomProcess(processName, config_.gameGateProcesses);
    }
    engine_.RefreshLongPressForeground(foregroundWindow, allowed);
}

void MainWindow::openExportProfileDialog() {
    SharePayload payload;
    payload.faction = config_.faction;
    if (auto* profile = profileController_.activeProfile()) {
        payload.profiles.push_back(*profile);
    }
    ShareExportDialog dialog(
        QString::fromWCharArray(L"\u5bfc\u51fa\u6309\u952e\u65b9\u6848"), payload, this,
        [this](const QString& text) { suppressShareText(text); });
    dialog.exec();
}

void MainWindow::openExportMacroDialog() {
    SharePayload payload;
    if (auto* macro = macroController_.activeMacro()) {
        payload.macros.push_back(*macro);
    }
    ShareExportDialog dialog(
        QString::fromWCharArray(L"\u5bfc\u51fa\u5b8f"), payload, this,
        [this](const QString& text) { suppressShareText(text); });
    dialog.exec();
}

bool MainWindow::isShareTextSuppressed(const QString& text) const {
    return suppressedShareTexts_.contains(text);
}

void MainWindow::suppressShareText(const QString& text) {
    if (text.isEmpty()) {
        return;
    }
    suppressedShareTexts_.removeAll(text);
    suppressedShareTexts_.prepend(text);
    while (suppressedShareTexts_.size() > 16) {
        suppressedShareTexts_.removeLast();
    }
}

void MainWindow::openImportDialog(const QString& shareText) {
    view_.shareBanner()->hideBanner();
    const QString trimmed = shareText.trimmed();
    if (trimmed.isEmpty()) {
        return;
    }
    // 为这段内容弹过导入窗口就不再重复提示（无论之后导入、取消还是解析失败）。
    suppressShareText(trimmed);
    ShareImportDialog dialog(this);
    dialog.setText(shareText);
    if (dialog.exec() != QDialog::Accepted || !dialog.hasImport()) {
        return;
    }
    applyImportedSharePayload(dialog.importedPayload());
}

// 只在窗口激活或激活状态下剪贴板变化时嗅探前缀，不做后台剪贴板监控。
void MainWindow::checkClipboardForShareText() {
    if (QApplication::activeModalWidget() != nullptr) {
        return;
    }
    const QString text = QApplication::clipboard()->text().trimmed();
    if (text.isEmpty() || !ShareCodec::LooksLikeShareText(text.toStdString())) {
        return;
    }
    if (isShareTextSuppressed(text)) {
        return;
    }
    if (view_.shareBanner()->shareText() == text) {
        return;
    }
    view_.shareBanner()->showForText(text);
}

void MainWindow::applyImportedSharePayload(const SharePayload& payload) {
    bool structureChanged = false;

    engineStateCoordinator_.setRepeatRunning(false);
    macroEngine_.StopAll();

    std::uint64_t nextProfileId = profileController_.nextProfileId();
    for (auto profile : payload.profiles) {
        profile.id = nextProfileId++;
        std::uint64_t bindingId = 0;
        for (auto& binding : profile.bindings) {
            binding.id = ++bindingId;
        }
        for (const auto& existing : config_.profiles) {
            if (existing.name == profile.name) {
                profile.name += L"(\u5bfc\u5165)";
                break;
            }
        }
        config_.profiles.push_back(std::move(profile));
        structureChanged = true;
    }

    std::uint64_t nextMacroId = 1;
    for (const auto& existing : config_.macros) {
        nextMacroId = std::max(nextMacroId, existing.id + 1);
    }
    for (auto macro : payload.macros) {
        macro.id = nextMacroId++;
        std::uint64_t stepId = 0;
        for (auto& step : macro.steps) {
            step.id = ++stepId;
        }
        for (const auto& existing : config_.macros) {
            if (existing.name == macro.name) {
                macro.name += L"(\u5bfc\u5165)";
                break;
            }
        }
        config_.macros.push_back(std::move(macro));
        structureChanged = true;
    }

    // 配方携带门派主题时跟随切换（仅导入方案时）。
    if (!payload.profiles.empty() && !payload.faction.empty()) {
        const QString factionId = QString::fromStdString(payload.faction);
        if (factionId != QString::fromStdString(config_.faction)) {
            config_.faction = payload.faction;
            config_.crispyThemeEnabled = false;
            FactionTheme::instance().setCurrent(factionId);
            view_.updateFactionVisuals(this);
        }
    }

    if (!structureChanged) {
        return;
    }

    configCoordinator_.save();
    engineStateCoordinator_.applyBindings();
    engineStateCoordinator_.applyMacros();
    pageRefreshCoordinator_.refreshAll();
    refreshStatusOverlay();
}

void MainWindow::showDdUnavailableMessage() {
    if (pluginService_) {
        QString error;
        if (!pluginService_->available(QString::fromStdString(config_.activeDriverId), &error)) {
            view_.pluginCenter()->setStatus(error);
            view_.navigateTo(QStringLiteral("plugins"));
            return;
        }
    }
    if (suppressDdUnavailablePrompt_ || driverUninstallInProgress_
        || activeDriverUninstalledByUser()) {
        return;
    }

    logDriverUnavailableOnce();
    // 启停热键/启动按钮撞上不可用驱动：弹一次自检向导，替代完全静默。
    openSelfCheckAfterDriverFailure();
}

void MainWindow::openSelfCheckAfterDriverFailure() {
    if (selfCheckAutoOpened_ || driverUninstallInProgress_
        || activeDriverUninstalledByUser()) {
        return;
    }
    if (!pluginService_ || !pluginService_->driverUi(QString::fromStdString(config_.activeDriverId)).value("selfCheck").toBool()) return;
    selfCheckAutoOpened_ = true;
    // 延迟到当前事件循环结束，让安装/切换的进度弹窗先收尾、热键处理先返回，再弹自检向导。
    const auto promptGeneration = selfCheckPromptGeneration_;
    QTimer::singleShot(0, this, [this, promptGeneration]() {
        if (promptGeneration != selfCheckPromptGeneration_ || driverUninstallInProgress_
            || inputHook_.isDialogOpen() || activeDriverUninstalledByUser()) return;
        driverSelfCheckCoordinator_.open();
    });
}

void MainWindow::logDriverUnavailableOnce() {
    if (driverUnavailableLogged_) {
        return;
    }
    driverUnavailableLogged_ = true;

    const auto status = ddRuntimeService_.Inspect(activeInputDriverReady(), activeInputDriverError());
    std::wstringstream errorLog;
    errorLog << L"driver_provider=" << QString::fromStdString(config_.activeDriverId).toStdWString() << L"\n"
             << L"last_error=" << (activeInputDriverError().empty() ? L"(none)" : activeInputDriverError()) << L"\n"
             << L"runtime_error=" << (activeInputDriverError().empty() ? L"(none)" : activeInputDriverError()) << L"\n"
             << L"xiaoji_keyboard=" << status.xiaojiKeyboardReady << L"\n"
             << L"xiaoji_mouse=" << status.xiaojiMouseReady << L"\n"
             << L"inf_keyboard_service=" << status.infKeyboardServiceInstalled << L"\n"
             << L"inf_mouse_service=" << status.infMouseServiceInstalled << L"\n"
             << L"inf_keyboard_device="
             << (status.infKeyboardDeviceStarted && status.infKeyboardDeviceBound) << L"\n"
             << L"inf_mouse_device="
             << (status.infMouseDeviceStarted && status.infMouseDeviceBound) << L"\n"
             << L"status_note=" << (driverStatusNote_.empty() ? L"(none)" : driverStatusNote_) << L"\n";
    // 内核隔离轻量检测：售后拿到日志即可直接看到“连发无效”的最常见根因。
    const auto guards = CodeIntegrityInspector::Inspect(false);
    errorLog << L"guard_hvci_running=" << guards.memoryIntegrityRunning << L"\n"
             << L"guard_hvci_configured=" << guards.memoryIntegrityConfigured << L"\n"
             << L"guard_vulnerable_blocklist=" << guards.vulnerableBlocklistEnabled << L"\n"
             << L"guard_smart_app_control=" << guards.smartAppControlState << L"\n";
    ddRuntimeService_.LogRuntimeError(L"dd-runtime.log", errorLog.str());
}
