#include "MainWindow.h"

#include "dialogs/AppMessageDialog.h"
#include "dialogs/ConfigConfirmDialog.h"
#include "dialogs/DialogChrome.h"
#include "dialogs/ShareExportDialog.h"
#include "dialogs/ShareImportDialog.h"
#include "FactionTheme.h"
#include "services/AgentService.h"
#include "ui/pages/DriverPage.h"
#include "ui/pages/SettingsPage.h"
#include "ui/styles/DesignTokens.h"

#include <QAbstractButton>
#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QDialog>
#include <QEvent>
#include <QFont>
#include <QLabel>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QProcess>
#include <QProgressBar>
#include <QResizeEvent>
#include <QScreen>
#include <QString>
#include <QThread>
#include <QTimer>
#include <QtGlobal>
#include <QVBoxLayout>
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

struct HidInstallResult {
    bool ok = false;
    std::wstring message;
};

class DriverProgressDialog final : public QDialog {
public:
    DriverProgressDialog(const QString& title, const QString& message, QWidget* parent)
        : QDialog(parent) {
        setWindowTitle(title);
        DialogChrome::apply(this);
        setModal(true);
        setWindowModality(Qt::ApplicationModal);
        setFixedSize(420, 168);

        auto* root = new QVBoxLayout(this);
        root->setContentsMargins(24, 20, 24, 20);
        root->setSpacing(12);

        auto* titleLabel = new QLabel(title, this);
        titleLabel->setFont(Fluent::Typography::cardTitle());
        titleLabel->setStyleSheet(QString("color: %1;").arg(Fluent::Colors::textPrimary().name()));
        root->addWidget(titleLabel);

        messageLabel_ = new QLabel(message, this);
        messageLabel_->setFont(Fluent::Typography::body());
        messageLabel_->setWordWrap(true);
        messageLabel_->setStyleSheet(QString("color: %1;").arg(Fluent::Colors::textSecondary().name()));
        root->addWidget(messageLabel_);

        progressBar_ = new QProgressBar(this);
        progressBar_->setRange(0, 100);
        progressBar_->setValue(8);
        progressBar_->setTextVisible(true);
        progressBar_->setFixedHeight(22);
        progressBar_->setStyleSheet(QString(
            "QProgressBar { border: 1px solid %1; border-radius: 6px; background: %2; color: %3; text-align: center; }"
            "QProgressBar::chunk { border-radius: 5px; background: %4; }")
            .arg(Fluent::Colors::border().name(QColor::HexArgb),
                 Fluent::Colors::sidebarBg().name(),
                 Fluent::Colors::textPrimary().name(),
                 Fluent::Colors::primary().name()));
        root->addWidget(progressBar_);

        connect(&timer_, &QTimer::timeout, this, [this]() {
            if (!progressBar_) return;
            const int current = progressBar_->value();
            progressBar_->setValue(qMin(current < 60 ? current + 3 : current + 1, 92));
        });
        timer_.start(260);
    }

    void markFinished(const QString& message) {
        timer_.stop();
        if (progressBar_) progressBar_->setValue(100);
        if (messageLabel_) messageLabel_->setText(message);
    }

    void setMessage(const QString& message) {
        if (messageLabel_) messageLabel_->setText(message);
    }

protected:
    void closeEvent(QCloseEvent* event) override {
        if (event) event->ignore();
    }

    void reject() override {}

    void paintEvent(QPaintEvent* event) override {
        DialogChrome::paint(this, event);
    }

    void mousePressEvent(QMouseEvent* event) override {
        if (DialogChrome::mousePress(this, event, dragging_, dragStartGlobal_, dragStartFrame_)) return;
        QDialog::mousePressEvent(event);
    }

    void mouseMoveEvent(QMouseEvent* event) override {
        if (DialogChrome::mouseMove(this, event, dragging_, dragStartGlobal_, dragStartFrame_)) return;
        QDialog::mouseMoveEvent(event);
    }

    void mouseReleaseEvent(QMouseEvent* event) override {
        if (DialogChrome::mouseRelease(event, dragging_)) return;
        QDialog::mouseReleaseEvent(event);
    }

private:
    QLabel* messageLabel_ = nullptr;
    QProgressBar* progressBar_ = nullptr;
    QTimer timer_;
    bool dragging_ = false;
    QPoint dragStartGlobal_;
    QPoint dragStartFrame_;
};

}

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent),
      appPaths_(),
      configCoordinator_(appPaths_.configPath(), this),
      profileController_(config_),
      macroController_(config_),
      engine_(inputDriver_),
      macroEngine_(inputDriver_),
      inputHook_(this),
      rawInput_(this),
      ddRuntimeService_(std::filesystem::path(appPaths_.exeDir()), appPaths_.logsDir()),
      soundService_(appPaths_.exeDir()),
      statusOverlay_(nullptr),
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
    connect(&usbHeartbeatTimer_, &QTimer::timeout, this, [this]() {
        if (config_.driverMode != DriverMode::Usb || !ch552Driver_.IsReady()) return;
        if (ch552Driver_.FirmwareVersion() != kBundledUsbDriverVersion ||
            ch552Driver_.Heartbeat()) return;
        (void)engineStateCoordinator_.setRepeatRunningFast(false);
        macroEngine_.StopAll();
        inputDriver_.ReleaseAll();
        inputDriver_.ClearAllStates();
        inputDriver_.SetDriver(nullptr);
        ch552Driver_.Disconnect();
        refreshStatusOverlay();
        usbDeviceRefreshTimer_.start(250);
    });
}

MainWindow::~MainWindow() {
    unregisterUsbDeviceNotifications();
    // 先停前台监视，避免析构期间回调再投递到已销毁的窗口。
    gameWatcher_.SetCallback(nullptr);
    gameWatcher_.Stop();
    engineStateCoordinator_.stopThreads();
    inputDriver_.ReleaseAll();
    inputDriver_.ClearAllStates();
}

bool MainWindow::Initialize() {
    setupUi();
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
    updateCoordinator_.restoreBadgeFromConfig();
    if (config_.backgroundAutoRun) {
        config_.backgroundAutoRun = false;
        configCoordinator_.save(false, 500);
    }
    // 开机自启：启动时对齐注册表 Run 键，修正 exe 移动或注册表被清理后的漂移。
    applyAutoStartSettings();
    // 后台兜底清理更新残留的 .old.exe（启动时的同步清理可能因旧进程
    // 未退出或文件被占用而失败）。
    UpdateService::retryOldExeCleanupInBackground(this);
    soundService_.warmUpToggleSounds(
        config_.customSoundOn,
        config_.customSoundOff,
        config_.macroSoundOn,
        config_.macroSoundOff);
    if (!loadConfiguredDriver()) {
        logDriverUnavailableOnce();
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
    applyGameGateSettings();
    gameWatcher_.Start();
    pageRefreshCoordinator_.refreshAll();
    refreshStatusOverlay();
    bindDpiRefreshSignals();
    observedScreenDpi_ = currentScreenDpi();
    QTimer::singleShot(250, this, [this]() {
        view_.initializeTray(this);
        hookTrayGameGateAction();
    });
    return true;
}

void MainWindow::ShowForStartup() {
    if (config_.startupAction == StartupAction::Tray) {
        view_.initializeTray(this);
        hookTrayGameGateAction();
        hide();
        return;
    }
    show();
}

bool MainWindow::loadConfiguredDriver(bool allowHidFallbackToLegacy) {
    driverStatusNote_.clear();
    driverStatusFallback_ = false;
    if (config_.driverMode == DriverMode::Usb) {
        if (loadCh552UsbDriver()) {
            driverUnavailableLogged_ = false;
            return true;
        }
        return false;
    }
    if (config_.driverMode == DriverMode::Hid) {
        if (loadHidDriver()) {
            driverUnavailableLogged_ = false;
            return true;
        }
        if (!allowHidFallbackToLegacy) {
            return false;
        }
        driverStatusNote_ = L"HID \u52a0\u8f7d\u5931\u8d25\uff0c\u5df2\u56de\u9000\u65e7 DD: " +
            (driver_.GetLastError().empty() ? std::wstring(L"(none)") : driver_.GetLastError());
        const bool legacyLoaded = loadLegacyDriver();
        if (legacyLoaded) {
            config_.driverMode = DriverMode::Legacy;
            configCoordinator_.save();
            driverUnavailableLogged_ = false;
            driverStatusFallback_ = true;
        }
        return legacyLoaded;
    }
    const bool legacyLoaded = loadLegacyDriver();
    if (legacyLoaded) {
        driverUnavailableLogged_ = false;
    }
    return legacyLoaded;
}

bool MainWindow::loadHidDriver() {
    ch552Driver_.Disconnect();
    driver_.ReleaseAll();
    driver_.Unload();

    const auto status = ddRuntimeService_.Inspect(driver_);
    const bool hidInstalled = status.hidServiceInstalled && status.hidSysInstalled &&
        (status.hidRootDeviceOk || status.hidChildDeviceOk);
    const std::wstring hidDllPath = (ddRuntimeService_.DriverRootDir() / L"hid" / L"ddhid.63340.dll").wstring();

    // Only load HID directly when Windows already reports the HID service and devices ready.
    if (status.hidRuntimeExists && hidInstalled) {
        if (driver_.LoadHidFrom(hidDllPath) &&
            driver_.GetRuntimeKind() == DdDriver::RuntimeKind::Hid) {
            inputDriver_.SetDriver(&driver_);
            return true;
        }

        if (driver_.GetRuntimeKind() != DdDriver::RuntimeKind::Unknown) {
            driver_.ReleaseAll();
            driver_.Unload();
        }
    }

    if (hidInstalled) {
        if (driverStatusNote_.empty()) {
            driverStatusNote_ = L"HID \u5df2\u68c0\u6d4b\u5230\u5df2\u5b89\u88c5\uff0c\u4f46\u5f53\u524d\u52a0\u8f7d\u5931\u8d25\uff0c\u672a\u91cd\u590d\u6267\u884c\u5b89\u88c5\u3002";
        }
        return false;
    }

    return false;
}

bool MainWindow::loadLegacyDriver() {
    ch552Driver_.Disconnect();
    driver_.ReleaseAll();
    driver_.Unload();
    ensureLegacyDdInfInstalled();
    const bool loaded = driver_.LoadFromDirectory(appPaths_.exeDir(), false);
    if (loaded) {
        inputDriver_.SetDriver(&driver_);
    }
    return loaded;
}

bool MainWindow::loadCh552UsbDriver() {
    driver_.ReleaseAll();
    driver_.Unload();
    ch552Driver_.Disconnect();
    const bool loaded = ch552Driver_.AutoConnect();
    if (loaded) {
        if (ch552Driver_.FirmwareVersion() == kBundledUsbDriverVersion &&
            !ch552Driver_.BeginLease()) {
            ch552Driver_.Disconnect();
            inputDriver_.SetDriver(nullptr);
            return false;
        }
        inputDriver_.SetDriver(&ch552Driver_);
    } else {
        inputDriver_.SetDriver(nullptr);
    }
    return loaded;
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
    if (config_.driverMode != DriverMode::Usb) {
        refreshDriverStatusUi();
        return;
    }

    engineStateCoordinator_.setRepeatRunning(false);
    macroEngine_.StopAll();
    inputDriver_.ReleaseAll();
    inputDriver_.ClearAllStates();
    driverStatusNote_.clear();
    driverStatusFallback_ = false;

    const bool loaded = loadCh552UsbDriver();
    if (loaded) {
        driverUnavailableLogged_ = false;
    }
    engineHookCoordinator_.refreshState();
    refreshDriverStatusUi();
    refreshStatusOverlay();
}

bool MainWindow::activeInputDriverReady() const {
    return inputDriver_.IsReady();
}

std::wstring MainWindow::activeInputDriverError() const {
    return inputDriver_.GetLastError();
}

bool MainWindow::loadLegacyDriverInto(DdDriver& targetDriver, std::wstring* errorMessage) {
    std::wstring installMessage;
    if (!ensureLegacyDdInfInstalled(&installMessage)) {
        if (errorMessage) {
            *errorMessage = installMessage.empty()
                ? std::wstring(L"DD INF driver is not available.")
                : installMessage;
        }
        return false;
    }

    if (!targetDriver.LoadFromDirectory(appPaths_.exeDir(), false)) {
        if (errorMessage) {
            *errorMessage = targetDriver.GetLastError();
        }
        targetDriver.ReleaseAll();
        targetDriver.Unload();
        return false;
    }
    return true;
}

bool MainWindow::canLoadCh552UsbDriver(std::wstring* errorMessage) {
    Ch552UsbDriver probeDriver;
    const bool loaded = probeDriver.AutoConnect();
    if (!loaded && errorMessage) {
        *errorMessage = probeDriver.GetLastError();
    }
    probeDriver.Disconnect();
    return loaded;
}

bool MainWindow::ensureLegacyDdInfInstalled(std::wstring* resultMessage) {
    const auto status = ddRuntimeService_.Inspect(driver_);
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

    const auto postInstallStatus = ddRuntimeService_.Inspect(driver_);
    return postInstallStatus.infKeyboardServiceInstalled &&
        postInstallStatus.infMouseServiceInstalled &&
        postInstallStatus.infKeyboardSysInstalled &&
        postInstallStatus.infMouseSysInstalled;
}

bool MainWindow::ensureHidDriverInstalledForSwitch() {
    const auto status = ddRuntimeService_.Inspect(driver_);
    const bool hidReady = status.hidRuntimeExists &&
        status.hidServiceInstalled &&
        status.hidSysInstalled &&
        (status.hidRootDeviceOk || status.hidChildDeviceOk);
    if (hidReady) {
        return true;
    }

    inputHook_.setDialogOpen(true);
    auto* progressDialog = new DriverProgressDialog(
        QString::fromWCharArray(L"\u5b89\u88c5 HID \u9a71\u52a8"),
        QString::fromWCharArray(L"\u6b63\u5728\u5b89\u88c5 HID \u9a71\u52a8\uff0c\u5b8c\u6210\u540e\u5c06\u91cd\u542f\u5c0f\u53fd\u3002"),
        this);
    const auto result = std::make_shared<HidInstallResult>();
    auto* thread = QThread::create([runtimeService = &ddRuntimeService_, result]() {
        result->ok = runtimeService->InstallEmbeddedHidDriver(&result->message);
    });
    thread->setParent(this);

    connect(thread, &QThread::finished, this, [this, progressDialog, thread, result]() {
        thread->deleteLater();
        if (progressDialog) {
            progressDialog->markFinished(result->ok
                ? QString::fromWCharArray(L"HID \u9a71\u52a8\u5b89\u88c5\u5b8c\u6210\uff0c\u6b63\u5728\u51c6\u5907\u91cd\u542f\u5c0f\u53fd...")
                : QString::fromWCharArray(L"HID \u9a71\u52a8\u5b89\u88c5\u5931\u8d25\u3002"));
            progressDialog->accept();
            progressDialog->deleteLater();
        }
        inputHook_.setDialogOpen(false);
    }, Qt::QueuedConnection);

    thread->start();
    progressDialog->exec();
    inputHook_.setDialogOpen(false);

    if (!result->ok) {
        ddRuntimeService_.LogRuntimeError(L"dd-runtime.log",
            L"HID install during driver switch failed.\n" +
            (result->message.empty() ? std::wstring(L"(none)") : result->message));
        AppMessageDialog::warning(
            this,
            QString::fromWCharArray(L"HID \u5b89\u88c5\u5931\u8d25"),
            QString::fromWCharArray(L"\u65e0\u6cd5\u5b89\u88c5 HID \u9a71\u52a8\uff0c\u5df2\u4fdd\u7559 DD \u9a71\u52a8\u3002\n\n\u8be6\u7ec6\u8fc7\u7a0b\u5df2\u5199\u5165 logs \u76ee\u5f55\u3002"));
        return false;
    }
    return true;
}

void MainWindow::refreshDriverStatusUi() {
    auto* page = view_.driverPage();
    if (!page) {
        return;
    }

    page->setDriverMode(config_.driverMode);
    const auto status = ddRuntimeService_.Inspect(driver_);

    QString summary = activeInputDriverReady()
        ? QString::fromWCharArray(L"\u53ef\u7528")
        : QString::fromWCharArray(L"\u4e0d\u53ef\u7528");
    if (driverStatusFallback_) {
        summary += QString::fromWCharArray(L" / \u5df2\u56de\u9000");
    }
    page->setStatusSummary(summary);
    const bool ddUninstallAvailable = status.infKeyboardServiceInstalled ||
        status.infMouseServiceInstalled || status.infKeyboardSysInstalled ||
        status.infMouseSysInstalled || status.infKeyboardDeviceStarted ||
        status.infMouseDeviceStarted || status.infKeyboardDeviceBound ||
        status.infMouseDeviceBound;
    const bool hidUninstallAvailable = status.hidServiceInstalled || status.hidServicePendingDelete ||
        status.hidSysInstalled || status.hidDriverStorePresent ||
        status.hidRootDeviceOk || status.hidChildDeviceOk;
    page->setDriverActions(ddUninstallAvailable, hidUninstallAvailable);

    std::wstringstream detail;
    detail << L"\u914d\u7f6e\u9a71\u52a8: ";
    if (config_.driverMode == DriverMode::Usb) detail << L"USB";
    else detail << (config_.driverMode == DriverMode::Hid ? L"HID" : L"DD");
    detail << L"\n";
    detail << L"\n";
    const bool usbCommunicationReady = ch552Driver_.IsReady();
    const std::wstring usbPortName = usbCommunicationReady
        ? ch552Driver_.PortName()
        : Ch552UsbDriver::DetectPortName();
    detail << L"USB \u8bbe\u5907: " << (!usbPortName.empty() ? L"\u5df2\u63d2\u5165" : L"\u672a\u63d2\u5165") << L"\n";
    detail << L"USB \u901a\u4fe1: ";
    if (usbCommunicationReady) {
        detail << L"\u5df2\u8fde\u63a5";
    } else if (config_.driverMode == DriverMode::Usb) {
        detail << L"\u8fde\u63a5\u5931\u8d25";
    } else {
        detail << L"\u672a\u542f\u7528\uff08\u5f53\u524d\u4f7f\u7528 "
               << (config_.driverMode == DriverMode::Hid ? L"HID" : L"DD") << L"\uff09";
    }
    detail << L"\n";
    if (usbCommunicationReady) {
        detail << L"USB \u56fa\u4ef6: " << ch552Driver_.FirmwareVersion() << L"\n";
    } else if (config_.driverMode == DriverMode::Usb && !ch552Driver_.GetLastError().empty()) {
        detail << L"USB \u9519\u8bef: " << ch552Driver_.GetLastError() << L"\n";
    }
    detail << L"\n";
    detail << L"HID DLL: " << (status.hidRuntimeExists ? L"\u5df2\u627e\u5230" : L"\u672a\u627e\u5230") << L"\n";
    detail << L"HID \u670d\u52a1: "
           << (status.hidServiceInstalled ? L"\u5df2\u5b89\u88c5" :
               (status.hidServicePendingDelete ? L"\u7b49\u5f85\u91cd\u542f" : L"\u672a\u5b89\u88c5"))
           << L"\n";
    detail << L"HID SYS: " << (status.hidSysInstalled ? L"\u5df2\u5b89\u88c5" : L"\u672a\u5b89\u88c5") << L"\n";
    detail << L"HID DriverStore: " << (status.hidDriverStorePresent ? L"\u4ecd\u5b58\u5728" : L"\u5df2\u79fb\u9664") << L"\n";
    const bool hidInstalled = status.hidServiceInstalled || status.hidSysInstalled || status.hidDriverStorePresent;
    const bool hidPendingRestart = status.hidServicePendingDelete && !status.hidSysInstalled && !status.hidDriverStorePresent;
    detail << L"HID Root \u8bbe\u5907: " << (status.hidRootDeviceOk ? L"OK" : (hidPendingRestart ? L"\u7b49\u5f85\u91cd\u542f" : (hidInstalled ? L"\u5f02\u5e38" : L"\u672a\u5b89\u88c5"))) << L"\n";
    detail << L"HID Child \u8bbe\u5907: " << (status.hidChildDeviceOk ? L"OK" : (hidPendingRestart ? L"\u7b49\u5f85\u91cd\u542f" : (hidInstalled ? L"\u5f02\u5e38" : L"\u672a\u5b89\u88c5"))) << L"\n\n";
    detail << L"DD32.dll: " << (status.legacy32Exists ? L"\u5df2\u627e\u5230" : L"\u672a\u627e\u5230") << L"\n";
    detail << L"DD64.dll: " << (status.legacy64Exists ? L"\u5df2\u627e\u5230" : L"\u672a\u627e\u5230") << L"\n";
    detail << L"INF \u952e\u76d8\u670d\u52a1: " << (status.infKeyboardServiceInstalled ? L"\u5df2\u5b89\u88c5" : L"\u672a\u5b89\u88c5") << L"\n";
    detail << L"INF \u9f20\u6807\u670d\u52a1: " << (status.infMouseServiceInstalled ? L"\u5df2\u5b89\u88c5" : L"\u672a\u5b89\u88c5") << L"\n";
    detail << L"INF \u952e\u76d8\u8bbe\u5907: " << ((status.infKeyboardDeviceStarted && status.infKeyboardDeviceBound) ? L"OK" : L"\u5f02\u5e38") << L"\n";
    detail << L"INF \u9f20\u6807\u8bbe\u5907: " << ((status.infMouseDeviceStarted && status.infMouseDeviceBound) ? L"OK" : L"\u5f02\u5e38") << L"\n";
    if (!status.lastError.empty()) {
        detail << L"\n\u9a71\u52a8\u9519\u8bef: " << status.lastError << L"\n";
    }
    if (!driverStatusNote_.empty()) {
        detail << L"\n\u5907\u6ce8: " << driverStatusNote_ << L"\n";
    }
    if (!status.notes.empty()) {
        detail << L"\n\u68c0\u67e5\u63d0\u793a:\n";
        for (const auto& note : status.notes) {
            detail << L"- " << note << L"\n";
        }
    }
    page->setDetailText(QString::fromStdWString(detail.str()));
}

DriverCoordinator::SwitchResult MainWindow::switchDriverMode(DriverMode mode) {
    const DriverMode originalMode = config_.driverMode;
    if (mode == originalMode) {
        refreshDriverStatusUi();
        return DriverCoordinator::SwitchResult::Succeeded;
    }

    const QString targetName = mode == DriverMode::Hid
        ? QStringLiteral("HID")
        : (mode == DriverMode::Usb ? QStringLiteral("USB") : QStringLiteral("DD"));
    inputHook_.setDialogOpen(true);
    const bool confirmed = ConfigConfirmDialog::confirm(
        this,
        QString::fromWCharArray(L"\u9a71\u52a8\u5207\u6362"),
        QString::fromWCharArray(L"\u5207\u6362\u5230 ") + targetName +
            QString::fromWCharArray(L" \u9700\u8981\u91cd\u542f\u5c0f\u53fd\uff0c\u70b9\u51fb\u786e\u5b9a\u540e\u5c06\u81ea\u52a8\u91cd\u65b0\u542f\u52a8\u3002"),
        QString::fromWCharArray(L"\u786e\u5b9a"));
    inputHook_.setDialogOpen(false);
    if (!confirmed) {
        refreshDriverStatusUi();
        return DriverCoordinator::SwitchResult::Canceled;
    }

    if (mode == DriverMode::Hid && !ensureHidDriverInstalledForSwitch()) {
        refreshDriverStatusUi();
        return DriverCoordinator::SwitchResult::Canceled;
    }

    if (mode == DriverMode::Legacy) {
        DdDriver stagedLegacyDriver;
        std::wstring legacyError;
        if (!loadLegacyDriverInto(stagedLegacyDriver, &legacyError)) {
            driverStatusNote_ = L"\u76ee\u6807 DD \u9a71\u52a8\u4e0d\u53ef\u7528\uff0c\u5df2\u4fdd\u7559\u5f53\u524d\u914d\u7f6e\u3002";
            if (!legacyError.empty()) {
                driverStatusNote_ += L" DD \u9519\u8bef: " + legacyError;
            }
            refreshDriverStatusUi();
            return DriverCoordinator::SwitchResult::Failed;
        }
    }
    if (mode == DriverMode::Usb) {
        std::wstring usbError;
        if (!canLoadCh552UsbDriver(&usbError)) {
            driverStatusNote_ = L"\u76ee\u6807 USB \u8bbe\u5907\u4e0d\u53ef\u7528\uff0c\u5df2\u4fdd\u7559\u5f53\u524d\u8fd0\u884c\u6001\u3002";
            if (!usbError.empty()) {
                driverStatusNote_ += L" USB \u9519\u8bef: " + usbError;
            }
            refreshDriverStatusUi();
            return DriverCoordinator::SwitchResult::Failed;
        }
    }

    driverStatusNote_.clear();
    driverStatusFallback_ = false;

    config_.driverMode = mode;
    configCoordinator_.save();
    if (!restartForDriverModeChange()) {
        config_.driverMode = originalMode;
        configCoordinator_.save();
        refreshDriverStatusUi();
        return DriverCoordinator::SwitchResult::Failed;
    }
    return DriverCoordinator::SwitchResult::Succeeded;
}

bool MainWindow::restartForDriverModeChange() {
    configCoordinator_.save();
    suppressDdUnavailablePrompt_ = true;

    const QString program = QCoreApplication::applicationFilePath();
    const QStringList arguments = { QStringLiteral("--restart-wait") };
    if (!QProcess::startDetached(program, arguments, QCoreApplication::applicationDirPath())) {
        suppressDdUnavailablePrompt_ = false;
        driverStatusNote_ = L"\u5207\u6362\u9a71\u52a8\u9700\u8981\u91cd\u542f\uff0c\u4f46\u65b0\u8fdb\u7a0b\u542f\u52a8\u5931\u8d25\u3002";
        return false;
    }

    QCoreApplication::quit();
    return true;
}

void MainWindow::prepareDriverUninstall(DriverMode mode) {
    engineStateCoordinator_.setRepeatRunning(false);
    macroEngine_.StopAll();
    inputDriver_.ReleaseAll();
    inputDriver_.ClearAllStates();

    if ((mode == DriverMode::Hid && driver_.GetRuntimeKind() == DdDriver::RuntimeKind::Hid) ||
        (mode == DriverMode::Legacy && driver_.GetRuntimeKind() == DdDriver::RuntimeKind::Legacy)) {
        driver_.ReleaseAll();
        driver_.Unload();
        inputDriver_.SetDriver(nullptr);
    }
}

void MainWindow::finalizeDriverUninstall(DriverMode mode) {
    if (config_.driverMode == mode) {
        inputDriver_.SetDriver(nullptr);
        if (mode != DriverMode::Usb) {
            driver_.Unload();
        }
    }
    if (mode == DriverMode::Hid && config_.driverMode == DriverMode::Hid) {
        config_.driverMode = DriverMode::Legacy;
        configCoordinator_.save();
        loadLegacyDriver();
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
    // Stop the running sessions and unload the driver DLLs so the package
    // files next to the exe can be replaced.
    (void)engineStateCoordinator_.setRepeatRunningFast(false);
    macroEngine_.StopAll();
    inputDriver_.ReleaseAll();
    inputDriver_.ClearAllStates();
    inputDriver_.SetDriver(nullptr);
    driver_.ReleaseAll();
    driver_.Unload();
    ch552Driver_.Disconnect();

    QString error;
    if (!UpdateService::applyStagedUpdate(stagingDir, &error)) {
        // Best effort: bring the driver back so the application stays usable.
        if (!loadConfiguredDriver(false)) {
            logDriverUnavailableOnce();
        }
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
    QCoreApplication::quit();
}

bool MainWindow::restartApplication(const QString& reasonMessage) {
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

    QCoreApplication::quit();
    return true;
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event) {
    if (event->type() == QEvent::MouseButtonPress && captureCoordinator_.captureActive()) {
        captureCoordinator_.cancelCapture();
    }
    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::resizeEvent(QResizeEvent* event) {
    QMainWindow::resizeEvent(event);
}

bool MainWindow::event(QEvent* event) {
    switch (event->type()) {
    case QEvent::ScreenChangeInternal:
        refreshIfScreenDpiChanged();
        break;
    default:
        break;
    }
    return QMainWindow::event(event);
}

bool MainWindow::nativeEvent(const QByteArray& eventType, void* message, long* result) {
    Q_UNUSED(eventType)
#ifdef Q_OS_WIN
    auto* msg = static_cast<MSG*>(message);
    if (msg) {
        if (msg->message == WM_INPUT) {
            rawInput_.handleNativeMessage(msg, result);
        }
        switch (msg->message) {
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
            if (isXiaoJiUsbDeviceChange(msg->wParam, msg->lParam)) {
                usbDeviceRefreshTimer_.start(650);
            }
            break;
        case WM_THEMECHANGED:
            Fluent::Theme::refreshDarkModeCache();
            scheduleRuntimeVisualRefresh(false, true);
            break;
        case WM_SETTINGCHANGE: {
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
    auto* profile = profileController_.activeProfile();
    auto* macro = macroController_.activeMacro();
    const bool repeatRunning = engine_.IsToolRunning();
    const bool macroRunning = macro ? macroEngine_.IsRunning(macro->id) : false;
    statusOverlay_.setGameGatePaused(
        config_.gameGateEnabled && !gameWatcher_.IsGameForeground()
        && (repeatRunning || macroRunning));
    if (config_.statusOverlayShowRepeat || config_.statusOverlayShowMacro) {
        const auto repeatStatus = config_.statusOverlayShowRepeat ? engine_.GetStatus() : RepeaterStatus{};
        const auto macroStatus = config_.statusOverlayShowMacro && macro ? macroEngine_.GetStatus(macro->id) : MacroStatus{};
        statusOverlay_.updateStatus(config_, profile, macro, repeatStatus, macroStatus);
    } else {
        statusOverlay_.setOverlayEnabled(false);
    }
    view_.updateActivityIcons(this, repeatRunning, macroRunning);
}

void MainWindow::applyGameGateSettings() {
    ForegroundWatcher::Rules rules;
    rules.enabled = config_.gameGateEnabled;
    rules.customProcesses = config_.gameGateProcesses;
    gameWatcher_.SetRules(std::move(rules));

    if (auto* action = view_.trayGameGateAction()) {
        action->blockSignals(true);
        action->setChecked(config_.gameGateEnabled);
        action->blockSignals(false);
    }
    if (auto* page = view_.settingsPage()) {
        page->setGameGateEnabled(config_.gameGateEnabled);
    }
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

void MainWindow::hookTrayGameGateAction() {
    if (trayGateHooked_) {
        return;
    }
    auto* action = view_.trayGameGateAction();
    if (action == nullptr) {
        return;
    }
    trayGateHooked_ = true;
    connect(action, &QAction::toggled, this, [this](bool checked) {
        config_.gameGateEnabled = checked;
        configCoordinator_.save();
        applyGameGateSettings();
    });
    applyGameGateSettings();
}

void MainWindow::openExportProfileDialog() {
    SharePayload payload;
    payload.faction = config_.faction;
    if (auto* profile = profileController_.activeProfile()) {
        payload.profiles.push_back(*profile);
    }
    ShareExportDialog dialog(
        QString::fromWCharArray(L"\u5bfc\u51fa\u6309\u952e\u65b9\u6848"), payload, this);
    dialog.exec();
}

void MainWindow::openExportMacroDialog() {
    SharePayload payload;
    if (auto* macro = macroController_.activeMacro()) {
        payload.macros.push_back(*macro);
    }
    ShareExportDialog dialog(
        QString::fromWCharArray(L"\u5bfc\u51fa\u5b8f"), payload, this);
    dialog.exec();
}

void MainWindow::openImportDialog() {
    ShareImportDialog dialog(this);
    if (dialog.exec() != QDialog::Accepted || !dialog.hasImport()) {
        return;
    }
    applyImportedSharePayload(dialog.importedPayload());
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
    if (suppressDdUnavailablePrompt_) {
        return;
    }

    logDriverUnavailableOnce();
}

void MainWindow::logDriverUnavailableOnce() {
    if (driverUnavailableLogged_) {
        return;
    }
    driverUnavailableLogged_ = true;

    const auto status = ddRuntimeService_.Inspect(driver_);
    std::wstringstream errorLog;
    errorLog << L"driver_mode=";
    if (config_.driverMode == DriverMode::Usb) errorLog << L"USB";
    else errorLog << (config_.driverMode == DriverMode::Hid ? L"HID" : L"DD");
    errorLog << L"\n"
             << L"last_error=" << (activeInputDriverError().empty() ? L"(none)" : activeInputDriverError()) << L"\n"
             << L"runtime_error=" << (driver_.GetLastError().empty() ? L"(none)" : driver_.GetLastError()) << L"\n"
             << L"runtime_kind=" << static_cast<int>(driver_.GetRuntimeKind()) << L"\n"
             << L"hid_runtime=" << status.hidRuntimeExists << L"\n"
             << L"hid_service=" << status.hidServiceInstalled << L"\n"
             << L"hid_sys=" << status.hidSysInstalled << L"\n"
             << L"hid_root_device=" << status.hidRootDeviceOk << L"\n"
             << L"hid_child_device=" << status.hidChildDeviceOk << L"\n"
             << L"inf_keyboard_service=" << status.infKeyboardServiceInstalled << L"\n"
             << L"inf_mouse_service=" << status.infMouseServiceInstalled << L"\n"
             << L"inf_keyboard_device="
             << (status.infKeyboardDeviceStarted && status.infKeyboardDeviceBound) << L"\n"
             << L"inf_mouse_device="
             << (status.infMouseDeviceStarted && status.infMouseDeviceBound) << L"\n"
             << L"status_note=" << (driverStatusNote_.empty() ? L"(none)" : driverStatusNote_) << L"\n";
    ddRuntimeService_.LogRuntimeError(L"dd-runtime.log", errorLog.str());
}
