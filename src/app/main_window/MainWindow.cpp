#include "MainWindow.h"

#include "dialogs/AppMessageDialog.h"
#include "dialogs/ConfigConfirmDialog.h"
#include "dialogs/DialogChrome.h"
#include "ui/pages/DriverPage.h"
#include "ui/pages/SettingsPage.h"
#include "ui/styles/DesignTokens.h"

#include <QAbstractButton>
#include <QApplication>
#include <QCloseEvent>
#include <QDialog>
#include <QDir>
#include <QEvent>
#include <QEventLoop>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QDateTime>
#include <QFont>
#include <QLabel>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QProcess>
#include <QProgressBar>
#include <QResizeEvent>
#include <QScreen>
#include <QString>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QThread>
#include <QTimer>
#include <QtGlobal>
#include <QVBoxLayout>
#include <QVersionNumber>
#include <QWindow>
#include <Windows.h>
#include <Dbt.h>
#include <Shellapi.h>
#include <SetupAPI.h>
#include <memory>
#include <cwchar>
#include <stdexcept>
#include <sstream>
#include <vector>

namespace {
constexpr int kBaseWindowWidth = 780;
constexpr int kBaseWindowHeight = 560;
constexpr int kBaseDpi = 96;
constexpr wchar_t kXiaoJiUsbVidPid[] = L"vid_1209&pid_584a";
constexpr wchar_t kCh552BootloaderVidPid[] = L"vid_4348&pid_55e0";
constexpr auto kEmbeddedUsbFirmwareResource = ":/firmware/xiaoji_usb.hex";
constexpr auto kEmbeddedUsbUpdaterResource = ":/firmware/updater/usb-updater.exe";
constexpr wchar_t kBundledUsbDriverVersion[] = L"XiaoJi USB V1.1.4";
const UINT kQueryCancelAutoPlayMessage = ::RegisterWindowMessageW(L"QueryCancelAutoPlay");
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

bool extractResource(const QString& resourcePath, const QString& targetPath, QString* errorMessage) {
    QFile source(resourcePath);
    if (!source.open(QIODevice::ReadOnly)) {
        if (errorMessage) {
            *errorMessage = QString::fromWCharArray(L"\u65e0\u6cd5\u8bfb\u53d6\u5185\u7f6e\u66f4\u65b0\u6587\u4ef6\uff1a") + resourcePath;
        }
        return false;
    }

    QFile target(targetPath);
    if (!target.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (errorMessage) {
            *errorMessage = QString::fromWCharArray(L"\u65e0\u6cd5\u521b\u5efa\u4e34\u65f6\u66f4\u65b0\u6587\u4ef6\uff1a") + target.errorString();
        }
        return false;
    }
    if (target.write(source.readAll()) < 0) {
        if (errorMessage) {
            *errorMessage = QString::fromWCharArray(L"\u65e0\u6cd5\u5199\u5165\u4e34\u65f6\u66f4\u65b0\u6587\u4ef6\uff1a") + target.errorString();
        }
        return false;
    }
    return true;
}

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
      nativeHidRuntimeService_(std::filesystem::path(appPaths_.exeDir()), appPaths_.logsDir()),
      soundService_(appPaths_.exeDir()),
      statusOverlay_(nullptr) {
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
    const bool repeatRunning = engine_.IsToolRunning();
    unregisterUsbDeviceNotifications();
    engineStateCoordinator_.stopThreads();
    inputDriver_.ReleaseAll();
    inputDriver_.ClearAllStates();
    if (config_.backgroundAutoRun && agentPausedForUi_) {
        AgentService::Resume(repeatRunning, nullptr);
        agentPausedForUi_ = false;
    }
}

bool isUsbDevicePresent(const wchar_t* vidPid) {
    HDEVINFO deviceInfo = ::SetupDiGetClassDevsW(
        nullptr, nullptr, nullptr, DIGCF_ALLCLASSES | DIGCF_PRESENT);
    if (deviceInfo == INVALID_HANDLE_VALUE) {
        return false;
    }

    bool found = false;
    for (DWORD index = 0; !found; ++index) {
        SP_DEVINFO_DATA deviceData{};
        deviceData.cbSize = sizeof(deviceData);
        if (!::SetupDiEnumDeviceInfo(deviceInfo, index, &deviceData)) {
            break;
        }

        DWORD propertyType = 0;
        DWORD requiredSize = 0;
        ::SetupDiGetDeviceRegistryPropertyW(
            deviceInfo, &deviceData, SPDRP_HARDWAREID, &propertyType,
            nullptr, 0, &requiredSize);
        if (requiredSize < sizeof(wchar_t)) {
            continue;
        }

        std::vector<BYTE> property(requiredSize);
        if (!::SetupDiGetDeviceRegistryPropertyW(
                deviceInfo, &deviceData, SPDRP_HARDWAREID, &propertyType,
                property.data(), static_cast<DWORD>(property.size()), nullptr)) {
            continue;
        }

        const wchar_t* hardwareId = reinterpret_cast<const wchar_t*>(property.data());
        while (*hardwareId != L'\0') {
            if (QString::fromWCharArray(hardwareId).contains(
                    QString::fromWCharArray(vidPid), Qt::CaseInsensitive)) {
                found = true;
                break;
            }
            hardwareId += std::wcslen(hardwareId) + 1;
        }
    }

    ::SetupDiDestroyDeviceInfoList(deviceInfo);
    return found;
}

bool isCurrentProcessElevated() {
    HANDLE token = nullptr;
    if (!::OpenProcessToken(::GetCurrentProcess(), TOKEN_QUERY, &token)) {
        return false;
    }
    TOKEN_ELEVATION elevation{};
    DWORD returned = 0;
    const bool elevated = ::GetTokenInformation(
        token, TokenElevation, &elevation, sizeof(elevation), &returned) != FALSE &&
        elevation.TokenIsElevated != 0;
    ::CloseHandle(token);
    return elevated;
}

bool installWinUsbBootloaderDriver(const QString& installerPath, const QString& driverPath,
    const QString& logPath, QString* errorMessage) {
    QString effectiveLogPath = logPath;
    QFileInfo requestedLogInfo(effectiveLogPath);
    QDir().mkpath(requestedLogInfo.absolutePath());
    QFile logProbe(effectiveLogPath);
    if (!logProbe.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        effectiveLogPath = QDir(
            QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation))
            .filePath(QStringLiteral("logs/winusb-install.log"));
    }
    logProbe.close();
    const QFileInfo logInfo(effectiveLogPath);
    QDir().mkpath(logInfo.absolutePath());
    auto appendLog = [&effectiveLogPath](const QString& text) {
        QFile log(effectiveLogPath);
        if (log.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
            log.write(text.toUtf8());
            log.write("\r\n");
        }
    };

    appendLog(QStringLiteral("[%1] Starting WinUSB bootloader driver installation")
        .arg(QDateTime::currentDateTime().toString(Qt::ISODate)));
    appendLog(QStringLiteral("INSTALLER=%1").arg(QDir::toNativeSeparators(installerPath)));
    appendLog(QStringLiteral("DRIVER_OUTPUT=%1").arg(QDir::toNativeSeparators(driverPath)));
    appendLog(QStringLiteral("LOG=%1").arg(QDir::toNativeSeparators(effectiveLogPath)));
    appendLog(QStringLiteral("Process elevated=%1").arg(isCurrentProcessElevated() ? QStringLiteral("1") : QStringLiteral("0")));

    // ShellExecuteEx with runas cannot redirect stdout directly.  Use a short
    // elevated cmd script so libwdi output survives the UAC child process.
    const QString scriptPath = logInfo.absoluteDir().filePath(
        QStringLiteral("winusb-install-%1.cmd").arg(QCoreApplication::applicationPid()));
    QFile script(scriptPath);
    if (!script.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        appendLog(QStringLiteral("Unable to create elevated helper: %1").arg(script.errorString()));
        if (errorMessage) {
            *errorMessage = QString::fromWCharArray(L"\u65e0\u6cd5\u521b\u5efa WinUSB \u5b89\u88c5\u8f85\u52a9\u811a\u672c\u3002\n\n\u65e5\u5fd7\uff1a") + effectiveLogPath;
        }
        return false;
    }
    QDir().mkpath(driverPath);
    const QByteArray installerForCmd = QDir::toNativeSeparators(installerPath).toUtf8();
    const QByteArray driverForCmd = QDir::toNativeSeparators(driverPath).toUtf8();
    const QByteArray logForCmd = QDir::toNativeSeparators(effectiveLogPath).toUtf8();
    QByteArray scriptContents = "@echo off\r\nsetlocal\r\n";
    scriptContents += "echo ==== libwdi output ==== >> \"" + logForCmd + "\"\r\n";
    scriptContents += "echo Command: xiaoji-winusb-installer.exe --vid 0x4348 --pid 0x55E0 --type 0 >> \"" + logForCmd + "\"\r\n";
    scriptContents += "\"" + installerForCmd + "\" --name \"XiaoJi CH552 Bootloader\" --manufacturer \"XiaoJi\" --vid 0x4348 --pid 0x55E0 --type 0 --dest \"" + driverForCmd + "\" --inf xiaoji-bootloader.inf --log 0 --timeout 60000 >> \"" + logForCmd + "\" 2>&1\r\n";
    scriptContents += "set WDI_EXIT=%ERRORLEVEL%\r\n";
    scriptContents += "echo LIBWDI_EXIT_CODE=%WDI_EXIT% >> \"" + logForCmd + "\"\r\n";
    scriptContents += "exit /b %WDI_EXIT%\r\n";
    script.write(scriptContents);
    script.close();

    const QString arguments = QStringLiteral("/d /c \"\"%1\"\"").arg(QDir::toNativeSeparators(scriptPath));
    std::wstring argumentsWide = arguments.toStdWString();
    SHELLEXECUTEINFOW executeInfo{};
    executeInfo.cbSize = sizeof(executeInfo);
    executeInfo.fMask = SEE_MASK_NOCLOSEPROCESS;
    executeInfo.lpVerb = L"runas";
    executeInfo.lpFile = L"cmd.exe";
    executeInfo.lpParameters = argumentsWide.c_str();
    executeInfo.nShow = SW_HIDE;
    if (!::ShellExecuteExW(&executeInfo)) {
        const DWORD shellError = ::GetLastError();
        appendLog(QStringLiteral("ShellExecuteExW failed: %1 (0x%2)")
            .arg(QString::number(shellError))
            .arg(QString::number(shellError, 16).toUpper()));
        if (errorMessage) {
            *errorMessage = QString::fromWCharArray(L"\u65e0\u6cd5\u4ee5\u7ba1\u7406\u5458\u6743\u9650\u542f\u52a8 WinUSB \u9a71\u52a8\u5b89\u88c5\u3002\n\n\u65e5\u5fd7\uff1a") + effectiveLogPath;
        }
        QFile::remove(scriptPath);
        return false;
    }

    const DWORD waitResult = ::WaitForSingleObject(executeInfo.hProcess, 120000);
    DWORD exitCode = ERROR_TIMEOUT;
    if (waitResult == WAIT_OBJECT_0) {
        if (!::GetExitCodeProcess(executeInfo.hProcess, &exitCode)) {
            exitCode = ::GetLastError();
            appendLog(QStringLiteral("GetExitCodeProcess failed: %1 (0x%2)")
                .arg(QString::number(exitCode))
                .arg(QString::number(exitCode, 16).toUpper()));
        }
    } else {
        appendLog(QStringLiteral("WaitForSingleObject result=%1").arg(QString::number(waitResult)));
    }
    ::CloseHandle(executeInfo.hProcess);
    appendLog(QStringLiteral("Helper exit code=%1 (0x%2)")
        .arg(QString::number(exitCode))
        .arg(QString::number(exitCode, 16).toUpper()));
    QFile::remove(scriptPath);
    if (waitResult != WAIT_OBJECT_0 || exitCode != ERROR_SUCCESS) {
        if (errorMessage) {
            *errorMessage = QString::fromWCharArray(L"WinUSB \u9a71\u52a8\u5b89\u88c5\u5931\u8d25\uff08\u9000\u51fa\u7801 ") +
                QString::number(static_cast<qulonglong>(exitCode)) +
                QString::fromWCharArray(L"\uff0c0x") +
                QString::number(static_cast<qulonglong>(exitCode), 16).toUpper() +
                QString::fromWCharArray(L"\uff09\u3002\n\n\u65e5\u5fd7\uff1a") + effectiveLogPath;
            *errorMessage += QString::fromWCharArray(
                L"\n\n\u8bf7\u67e5\u770b\u65e5\u5fd7\u4e2d\u7684 libwdi \u5b89\u88c5\u8be6\u7ec6\u8f93\u51fa\u3002");
        }
        return false;
    }
    return true;
}

bool MainWindow::Initialize() {
    setupUi();
    registerUsbDeviceNotifications();
    if (AgentService::IsRunning()) {
        agentPausedForUi_ = AgentService::PauseForUi(
            GetCurrentProcessId(), &agentRepeatRunningOnHandoff_, nullptr);
    }
    AgentService::RecoverFrontendConfig(appPaths_.configPath());
    AgentService::PublishFrontendConfigPath(appPaths_.configPath(), nullptr);
    configCoordinator_.load();
    const bool agentHandoffReady = initializeAgentHandoff();
    soundService_.warmUpToggleSounds(
        config_.customSoundOn,
        config_.customSoundOff,
        config_.macroSoundOn,
        config_.macroSoundOff);
    const bool usbHandoffFailed = config_.backgroundAutoRun &&
        config_.driverMode == DriverMode::Usb && !agentHandoffReady;
    if (!usbHandoffFailed) {
        if (!loadConfiguredDriver()) {
            logDriverUnavailableOnce();
        }
    } else {
        inputDriver_.SetDriver(nullptr);
        driverStatusNote_ = L"USB Agent handoff failed; repair it before using USB.";
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
    if (agentRepeatRunningOnHandoff_) {
        (void)engineStateCoordinator_.setRepeatRunningFast(true);
        agentRepeatRunningOnHandoff_ = false;
    }
    pageRefreshCoordinator_.refreshAll();
    refreshStatusOverlay();
    bindDpiRefreshSignals();
    observedScreenDpi_ = currentScreenDpi();
    QTimer::singleShot(250, this, [this]() {
        view_.initializeTray(this);
    });
    QTimer::singleShot(800, this, [this]() {
        checkUsbDriverUpdateOnStartup();
    });
    return true;
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
    if (config_.driverMode == DriverMode::Native) {
        if (loadNativeHidDriver()) {
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
    nativeHidDriver_.Disconnect();
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
    nativeHidDriver_.Disconnect();
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
    nativeHidDriver_.Disconnect();
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

bool MainWindow::loadNativeHidDriver() {
    ch552Driver_.Disconnect();
    driver_.ReleaseAll();
    driver_.Unload();
    nativeHidDriver_.Disconnect();
    const bool loaded = nativeHidDriver_.Connect();
    inputDriver_.SetDriver(loaded ? static_cast<InputDriver*>(&nativeHidDriver_) : nullptr);
    return loaded;
}

bool MainWindow::initializeAgentHandoff() {
    if (!config_.backgroundAutoRun) {
        AgentService::Uninstall(nullptr);
        agentPausedForUi_ = false;
        agentRepeatRunningOnHandoff_ = false;
        return true;
    }
    wchar_t executable[MAX_PATH] = {};
    const DWORD length = GetModuleFileNameW(nullptr, executable, MAX_PATH);
    if (length == 0) return false;
    std::wstring error;
    const auto failHandoff = [this]() {
        AgentService::Shutdown(nullptr);
        agentPausedForUi_ = false;
        agentRepeatRunningOnHandoff_ = false;
        return false;
    };
    configCoordinator_.save();
    if (!AgentService::PublishFrontendConfigPath(configCoordinator_.configPath(), &error)) {
        return failHandoff();
    }
    if (!AgentService::EnsureInstalled(std::filesystem::path(executable), &error)) {
        return failHandoff();
    }
    bool repeatRunning = false;
    if (!AgentService::PauseForUi(GetCurrentProcessId(), &repeatRunning, &error)) {
        return failHandoff();
    }
    agentRepeatRunningOnHandoff_ = agentRepeatRunningOnHandoff_ || repeatRunning;
    agentPausedForUi_ = true;
    return true;
}

void MainWindow::resumeAgent() {
    if (!agentPausedForUi_) return;
    AgentService::Resume(engine_.IsToolRunning(), nullptr);
    agentPausedForUi_ = false;
}

void MainWindow::setBackgroundAutoRun(bool enabled) {
    if (config_.backgroundAutoRun == enabled) return;
    config_.backgroundAutoRun = enabled;
    configCoordinator_.save();
    if (!enabled) {
        agentPausedForUi_ = false;
        AgentService::Uninstall(nullptr);
    } else {
        (void)initializeAgentHandoff();
    }
    refreshDriverStatusUi();
}

void MainWindow::repairAgent() {
    if (!config_.backgroundAutoRun) return;
    const bool ready = initializeAgentHandoff();
    if (ready && config_.driverMode == DriverMode::Usb && !activeInputDriverReady()) {
        (void)loadCh552UsbDriver();
        engineHookCoordinator_.refreshState();
        refreshStatusOverlay();
    }
    refreshDriverStatusUi();
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
        QTimer::singleShot(0, this, [this]() {
            checkUsbDriverUpdateOnStartup();
        });
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

bool MainWindow::ensureNativeHidInstalledForSwitch() {
    const auto status = nativeHidRuntimeService_.Inspect();
    if (status.serviceInstalled && status.sysInstalled && status.deviceStarted && status.interfacePresent) {
        return true;
    }

    inputHook_.setDialogOpen(true);
    auto* progressDialog = new DriverProgressDialog(
        QString::fromWCharArray(L"\u5b89\u88c5\u5c0f\u53fd HID"),
        QString::fromWCharArray(L"\u6b63\u5728\u5b89\u88c5\u5c0f\u53fd HID \u865a\u62df\u952e\u76d8\u9f20\u6807\u9a71\u52a8..."),
        this);
    const auto result = std::make_shared<HidInstallResult>();
    auto* thread = QThread::create([service = &nativeHidRuntimeService_, result]() {
        result->ok = service->Install(&result->message);
    });
    thread->setParent(this);

    connect(thread, &QThread::finished, this, [this, progressDialog, thread, result]() {
        thread->deleteLater();
        if (progressDialog) {
            progressDialog->markFinished(result->ok
                ? QString::fromWCharArray(L"\u5c0f\u53fd HID \u5b89\u88c5\u5b8c\u6210\uff0c\u6b63\u5728\u51c6\u5907\u5207\u6362...")
                : QString::fromWCharArray(L"\u5c0f\u53fd HID \u5b89\u88c5\u5931\u8d25\u3002"));
            progressDialog->accept();
            progressDialog->deleteLater();
        }
        inputHook_.setDialogOpen(false);
    }, Qt::QueuedConnection);

    thread->start();
    progressDialog->exec();
    inputHook_.setDialogOpen(false);

    if (!result->ok) {
        AppMessageDialog::warning(
            this,
            QString::fromWCharArray(L"\u5c0f\u53fd HID \u5b89\u88c5\u5931\u8d25"),
            QString::fromWCharArray(L"\u65e0\u6cd5\u5b89\u88c5\u5c0f\u53fd HID\u3002\u8bf7\u786e\u8ba4\u53d1\u5e03\u5305\u4e2d\u5305\u542b\u5df2\u7b7e\u540d\u7684 SYS/INF/CAT \u6587\u4ef6\u3002\n\n") +
                QString::fromStdWString(result->message));
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
    const auto nativeStatus = nativeHidRuntimeService_.Inspect();

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
    const bool nativeUninstallAvailable = nativeStatus.serviceInstalled || nativeStatus.sysInstalled ||
        nativeStatus.devicePresent || !nativeStatus.publishedInfName.empty();
    page->setDriverActions(
        ddUninstallAvailable, hidUninstallAvailable, nativeUninstallAvailable, isUsbDriverUpdateAvailable());
    const AgentStatus agentStatus = AgentService::QueryStatus();
    const bool agentHealthy = agentStatus.installed && agentStatus.taskRegistered && agentStatus.running;
    page->setAgentState(
        !config_.backgroundAutoRun
            ? QString::fromWCharArray(L"\u5df2\u5173\u95ed")
            : (agentStatus.firmwareUpdateRequired
                ? QString::fromWCharArray(L"\u9700\u66f4\u65b0 USB \u56fa\u4ef6")
            : (agentHealthy
                ? QString::fromWCharArray(agentStatus.pausedForUi ? L"\u524d\u53f0\u63a5\u7ba1\u4e2d" : L"\u8fd0\u884c\u4e2d")
                : QString::fromWCharArray(L"\u9700\u8981\u4fee\u590d"))),
        config_.backgroundAutoRun && !agentHealthy);

    std::wstringstream detail;
    detail << L"\u914d\u7f6e\u9a71\u52a8: ";
    if (config_.driverMode == DriverMode::Usb) detail << L"USB";
    else if (config_.driverMode == DriverMode::Native) detail << L"\u5c0f\u53fd HID";
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
               << (config_.driverMode == DriverMode::Hid ? L"HID" :
                   (config_.driverMode == DriverMode::Native ? L"\u5c0f\u53fd HID" : L"DD")) << L"\uff09";
    }
    detail << L"\n";
    if (usbCommunicationReady) {
        detail << L"USB \u56fa\u4ef6: " << ch552Driver_.FirmwareVersion() << L"\n";
    } else if (config_.driverMode == DriverMode::Usb && !ch552Driver_.GetLastError().empty()) {
        detail << L"USB \u9519\u8bef: " << ch552Driver_.GetLastError() << L"\n";
    }
    detail << L"\n";
    detail << L"\u5c0f\u53fd HID \u53d1\u5e03\u5305: " << (nativeStatus.packageExists ? L"\u5df2\u627e\u5230" : L"\u672a\u627e\u5230") << L"\n";
    detail << L"\u5c0f\u53fd HID \u670d\u52a1: " << (nativeStatus.serviceInstalled ? L"\u5df2\u5b89\u88c5" : L"\u672a\u5b89\u88c5") << L"\n";
    detail << L"\u5c0f\u53fd HID SYS: " << (nativeStatus.sysInstalled ? L"\u5df2\u5b89\u88c5" : L"\u672a\u5b89\u88c5") << L"\n";
    detail << L"\u5c0f\u53fd HID \u8bbe\u5907: " << (nativeStatus.deviceStarted ? L"OK" :
        (nativeStatus.devicePresent ? L"\u5f02\u5e38" : L"\u672a\u5b89\u88c5")) << L"\n";
    detail << L"\u5c0f\u53fd HID \u901a\u4fe1: " << (nativeStatus.interfacePresent ? L"OK" : L"\u4e0d\u53ef\u7528") << L"\n";
    if (!nativeStatus.publishedInfName.empty()) {
        detail << L"\u5c0f\u53fd HID INF: " << nativeStatus.publishedInfName << L"\n";
    }
    if (nativeHidDriver_.IsReady()) {
        detail << L"\u5c0f\u53fd HID \u7248\u672c: " << nativeHidDriver_.DriverVersion() << L"\n";
    } else if (config_.driverMode == DriverMode::Native && !nativeHidDriver_.GetLastError().empty()) {
        detail << L"\u5c0f\u53fd HID \u9519\u8bef: " << nativeHidDriver_.GetLastError() << L"\n";
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

bool MainWindow::isUsbDriverUpdateAvailable() const {
    if (config_.driverMode != DriverMode::Usb || !ch552Driver_.IsReady()) {
        return false;
    }
    const QString current = QString::fromStdWString(ch552Driver_.FirmwareVersion()).trimmed();
    if (current.isEmpty()) {
        return false;
    }
    if (current.contains(QStringLiteral("XiaoJi USB Bridge"), Qt::CaseInsensitive)) {
        return true;
    }
    const QString bundled = QString::fromWCharArray(kBundledUsbDriverVersion);
    const QVersionNumber currentVersion = QVersionNumber::fromString(current.mid(current.lastIndexOf(QLatin1Char('V')) + 1));
    const QVersionNumber bundledVersion = QVersionNumber::fromString(bundled.mid(bundled.lastIndexOf(QLatin1Char('V')) + 1));
    if (currentVersion.isNull() || bundledVersion.isNull()) {
        return false;
    }
    return QVersionNumber::compare(bundledVersion, currentVersion) > 0;
}

void MainWindow::checkUsbDriverUpdateOnStartup() {
    if (startupUsbUpdatePromptShown_) {
        return;
    }
    if (!isUsbDriverUpdateAvailable()) {
        return;
    }
    startupUsbUpdatePromptShown_ = true;

    const bool confirmed = ConfigConfirmDialog::confirm(
        this,
        QString::fromWCharArray(L"USB \u9a71\u52a8\u66f4\u65b0"),
        QString::fromWCharArray(L"\u68c0\u6d4b\u5230 USB \u9a71\u52a8\u6709\u65b0\u7248\u672c\uff0c\u662f\u5426\u73b0\u5728\u66f4\u65b0\uff1f"),
        QString::fromWCharArray(L"\u66f4\u65b0"));
    if (confirmed) {
        updateUsbDriver(false);
    }
}

DriverCoordinator::SwitchResult MainWindow::switchDriverMode(DriverMode mode) {
    const DriverMode originalMode = config_.driverMode;
    if (mode == originalMode) {
        refreshDriverStatusUi();
        return DriverCoordinator::SwitchResult::Succeeded;
    }

    const QString targetName = mode == DriverMode::Hid
        ? QStringLiteral("HID")
        : (mode == DriverMode::Usb
            ? QStringLiteral("USB")
            : (mode == DriverMode::Native ? QString::fromWCharArray(L"\u5c0f\u53fd HID") : QStringLiteral("DD")));
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
    if (mode == DriverMode::Native && !ensureNativeHidInstalledForSwitch()) {
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
    if (mode == DriverMode::Native) {
        NativeHidDriver probeDriver;
        if (!probeDriver.Connect()) {
            driverStatusNote_ = L"\u76ee\u6807\u5c0f\u53fd HID \u4e0d\u53ef\u7528\uff0c\u5df2\u4fdd\u7559\u5f53\u524d\u8fd0\u884c\u6001\u3002";
            const std::wstring nativeError = probeDriver.GetLastError();
            if (!nativeError.empty()) {
                driverStatusNote_ += L" Native \u9519\u8bef: " + nativeError;
            }
            refreshDriverStatusUi();
            return DriverCoordinator::SwitchResult::Failed;
        }
        probeDriver.Disconnect();
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

    if (mode == DriverMode::Native) {
        nativeHidDriver_.Disconnect();
        inputDriver_.SetDriver(nullptr);
    }

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
        if (mode != DriverMode::Usb && mode != DriverMode::Native) {
            driver_.Unload();
        }
    }
    if (mode == DriverMode::Hid && config_.driverMode == DriverMode::Hid) {
        config_.driverMode = DriverMode::Legacy;
        configCoordinator_.save();
        loadLegacyDriver();
    }
    if (mode == DriverMode::Native && config_.driverMode == DriverMode::Native) {
        config_.driverMode = DriverMode::Legacy;
        configCoordinator_.save();
        loadLegacyDriver();
    }
    engineHookCoordinator_.refreshState();
    refreshDriverStatusUi();
}

void MainWindow::updateUsbDriver(bool requireConfirmation) {
    QFile firmware(kEmbeddedUsbFirmwareResource);
    QFile updater(kEmbeddedUsbUpdaterResource);
    QFile winUsbInstaller(QStringLiteral(":/firmware/updater/xiaoji-winusb-installer.exe"));
    if (!firmware.exists() || !updater.exists() || !winUsbInstaller.exists()) {
        AppMessageDialog::warning(
            this,
            QString::fromWCharArray(L"USB \u9a71\u52a8\u66f4\u65b0\u5931\u8d25"),
            QString::fromWCharArray(L"\u7a0b\u5e8f\u5185\u7f6e\u7684 USB \u9a71\u52a8\u66f4\u65b0\u6587\u4ef6\u4e0d\u5b58\u5728\u3002"));
        return;
    }

    if (requireConfirmation) {
        const bool confirmed = ConfigConfirmDialog::confirm(
            this,
            QString::fromWCharArray(L"\u66f4\u65b0 USB \u9a71\u52a8"),
            QString::fromWCharArray(L"\u5c06\u81ea\u52a8\u66f4\u65b0 USB \u9a71\u52a8\u3002\u66f4\u65b0\u8fc7\u7a0b\u4e2d\u8bf7\u4e0d\u8981\u62d4\u51fa USB\u3002\u662f\u5426\u7ee7\u7eed\uff1f"),
            QString::fromWCharArray(L"\u66f4\u65b0"));
        if (!confirmed) {
            return;
        }
    }

    inputHook_.setDialogOpen(true);
    suppressAutoPlay_ = true;
    auto* progressDialog = new DriverProgressDialog(
        QString::fromWCharArray(L"\u66f4\u65b0 USB \u9a71\u52a8"),
        QString::fromWCharArray(L"\u6b63\u5728\u51c6\u5907 USB \u8bbe\u5907..."),
        this);
    progressDialog->show();
    QCoreApplication::processEvents(QEventLoop::AllEvents, 50);

    engineStateCoordinator_.setRepeatRunning(false);
    macroEngine_.StopAll();
    inputDriver_.ReleaseAll();
    inputDriver_.ClearAllStates();

    if (!ch552Driver_.IsReady() && !loadCh552UsbDriver()) {
        progressDialog->accept();
        progressDialog->deleteLater();
        inputHook_.setDialogOpen(false);
        suppressAutoPlay_ = false;
        AppMessageDialog::warning(
            this,
            QString::fromWCharArray(L"USB \u9a71\u52a8\u66f4\u65b0\u5931\u8d25"),
            QString::fromWCharArray(L"\u672a\u68c0\u6d4b\u5230\u53ef\u7528\u7684 USB \u8bbe\u5907\u3002"));
        refreshDriverStatusUi();
        return;
    }

    QTemporaryDir updateDirectory(QDir::tempPath() + QStringLiteral("/XiaoJiUsbUpdate-XXXXXX"));
    QString preparationError;
    const QString firmwarePath = updateDirectory.filePath(QStringLiteral("xiaoji_usb.hex"));
    const QString updaterPath = updateDirectory.filePath(QStringLiteral("usb-updater.exe"));
    const QString winUsbInstallerPath = updateDirectory.filePath(QStringLiteral("xiaoji-winusb-installer.exe"));
    const QString winUsbDriverPath = updateDirectory.filePath(QStringLiteral("winusb-driver"));
    const bool updateFilesReady = updateDirectory.isValid() &&
        extractResource(QStringLiteral(":/firmware/xiaoji_usb.hex"), firmwarePath, &preparationError) &&
        extractResource(QStringLiteral(":/firmware/updater/usb-updater.exe"), updaterPath, &preparationError) &&
        extractResource(QStringLiteral(":/firmware/updater/CH375DLL.dll"), updateDirectory.filePath(QStringLiteral("CH375DLL.dll")), &preparationError) &&
        extractResource(QStringLiteral(":/firmware/updater/libusb-1.0.dll"), updateDirectory.filePath(QStringLiteral("libusb-1.0.dll")), &preparationError) &&
        extractResource(QStringLiteral(":/firmware/updater/libgcc_s_sjlj-1.dll"), updateDirectory.filePath(QStringLiteral("libgcc_s_sjlj-1.dll")), &preparationError) &&
        extractResource(QStringLiteral(":/firmware/updater/xiaoji-winusb-installer.exe"), winUsbInstallerPath, &preparationError);
    if (!updateFilesReady) {
        progressDialog->accept();
        progressDialog->deleteLater();
        inputHook_.setDialogOpen(false);
        suppressAutoPlay_ = false;
        AppMessageDialog::warning(
            this,
            QString::fromWCharArray(L"USB \u9a71\u52a8\u66f4\u65b0\u5931\u8d25"),
            preparationError.isEmpty()
                ? QString::fromWCharArray(L"\u65e0\u6cd5\u51c6\u5907 USB \u9a71\u52a8\u66f4\u65b0\u6587\u4ef6\u3002")
                : preparationError);
        refreshDriverStatusUi();
        return;
    }

    const QString winUsbLogPath = QString::fromStdWString(
        (appPaths_.logsDir() / L"winusb-install.log").wstring());
    QString winUsbError;
    if (!installWinUsbBootloaderDriver(winUsbInstallerPath, winUsbDriverPath, winUsbLogPath, &winUsbError)) {
        progressDialog->accept();
        progressDialog->deleteLater();
        inputHook_.setDialogOpen(false);
        suppressAutoPlay_ = false;
        AppMessageDialog::warning(
            this,
            QString::fromWCharArray(L"USB \u9a71\u52a8\u66f4\u65b0\u5931\u8d25"),
            QString::fromWCharArray(L"\u9700\u8981\u5b89\u88c5 CH552 \u4e0b\u8f7d\u6a21\u5f0f WinUSB \u9a71\u52a8\uff0c\u4f46\u5b89\u88c5\u672a\u5b8c\u6210\u3002\n\n") + winUsbError);
        return;
    }

    if (!ch552Driver_.EnterBootloaderForUpdate()) {
        const QString error = QString::fromStdWString(ch552Driver_.GetLastError());
        progressDialog->accept();
        progressDialog->deleteLater();
        inputHook_.setDialogOpen(false);
        suppressAutoPlay_ = false;
        AppMessageDialog::warning(
            this,
            QString::fromWCharArray(L"USB \u9a71\u52a8\u66f4\u65b0\u5931\u8d25"),
            QString::fromWCharArray(L"USB \u8bbe\u5907\u672a\u54cd\u5e94\u66f4\u65b0\u8bf7\u6c42\u3002\n\n") + error);
        refreshDriverStatusUi();
        return;
    }

    inputDriver_.SetDriver(nullptr);
    progressDialog->setMessage(QString::fromWCharArray(L"\u6b63\u5728\u7b49\u5f85 USB \u8fdb\u5165\u66f4\u65b0\u6a21\u5f0f..."));
    QThread::msleep(1200);

    QProcess updateProcess;
    updateProcess.setWorkingDirectory(updateDirectory.path());
    updateProcess.setProcessChannelMode(QProcess::MergedChannels);
#ifdef Q_OS_WIN
    updateProcess.setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments* arguments) {
        arguments->flags |= CREATE_NO_WINDOW;
    });
#endif
    updateProcess.setProgram(updaterPath);
    updateProcess.setArguments({
        QStringLiteral("-r"), QStringLiteral("10"),
        QStringLiteral("-t"), QStringLiteral("CH552"),
        QStringLiteral("-c"), QStringLiteral("3"),
        firmwarePath
    });
    updateProcess.start();
    const bool updaterStarted = updateProcess.waitForStarted(3000);
    QElapsedTimer updateTimer;
    updateTimer.start();
    while (updaterStarted && updateProcess.state() != QProcess::NotRunning && updateTimer.elapsed() < 30000) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
        updateProcess.waitForFinished(100);
    }
    if (updateProcess.state() != QProcess::NotRunning) {
        updateProcess.kill();
        updateProcess.waitForFinished(1000);
    }
    const QString updateOutput = QString::fromLocal8Bit(updateProcess.readAll()).trimmed();
    const bool updateSucceeded = updaterStarted &&
        updateProcess.exitStatus() == QProcess::NormalExit &&
        updateProcess.exitCode() == 0;
    if (!updateSucceeded) {
        const bool bootloaderPresent = isUsbDevicePresent(kCh552BootloaderVidPid);
        ddRuntimeService_.LogRuntimeError(
            L"usb-update.log",
            L"USB updater failed. started=" + std::to_wstring(updaterStarted) +
                L", exit_code=" + std::to_wstring(updateProcess.exitCode()) +
                L", bootloader_present=" + (bootloaderPresent ? std::wstring(L"1") : std::wstring(L"0")) + L"\n" +
                (updateOutput.isEmpty() ? std::wstring(L"(no output)") : updateOutput.toStdWString()));
        progressDialog->accept();
        progressDialog->deleteLater();
        inputHook_.setDialogOpen(false);
        suppressAutoPlay_ = false;
        engineHookCoordinator_.refreshState();
        refreshDriverStatusUi();
        AppMessageDialog::warning(
            this,
            QString::fromWCharArray(L"USB \u9a71\u52a8\u66f4\u65b0\u5931\u8d25"),
            bootloaderPresent
                ? QString::fromWCharArray(L"\u5df2\u68c0\u6d4b\u5230 CH552 \u4e0b\u8f7d\u6a21\u5f0f\uff0c\u4f46 WinUSB \u5237\u5199\u901a\u9053\u65e0\u6cd5\u6253\u5f00\u3002\n\n\u8bf7\u68c0\u67e5 Windows \u8bbe\u5907\u7ba1\u7406\u5668\u4e2d\u7684 WinUSB \u7ed1\u5b9a\u3002\u8be6\u7ec6\u4fe1\u606f\u5df2\u5199\u5165 logs \u76ee\u5f55\u3002")
                : QString::fromWCharArray(L"\u672a\u80fd\u5b8c\u6210 USB \u9a71\u52a8\u5199\u5165\uff0c\u4e14\u672a\u68c0\u6d4b\u5230 CH552 \u4e0b\u8f7d\u6a21\u5f0f\u3002\u8bf7\u91cd\u63d2 USB \u8bbe\u5907\u540e\u91cd\u8bd5\u3002\n\n\u8be6\u7ec6\u4fe1\u606f\u5df2\u5199\u5165 logs \u76ee\u5f55\u3002"));
        return;
    }

    progressDialog->markFinished(QString::fromWCharArray(L"USB \u9a71\u52a8\u5df2\u5199\u5165\uff0c\u6b63\u5728\u7b49\u5f85\u8bbe\u5907\u91cd\u65b0\u8fde\u63a5..."));
    QElapsedTimer reconnectTimer;
    reconnectTimer.start();
    bool reconnected = false;
    while (reconnectTimer.elapsed() < 12000) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
        QThread::msleep(250);
        if (loadCh552UsbDriver()) {
            reconnected = true;
            break;
        }
    }

    inputHook_.setDialogOpen(false);
    suppressAutoPlay_ = false;
    progressDialog->accept();
    progressDialog->deleteLater();
    engineHookCoordinator_.refreshState();
    refreshDriverStatusUi();
    refreshStatusOverlay();

    const bool versionVerified = reconnected && !isUsbDriverUpdateAvailable();
    if (versionVerified) {
        AppMessageDialog::information(
            this,
            QString::fromWCharArray(L"USB \u9a71\u52a8\u66f4\u65b0\u5b8c\u6210"),
            QString::fromWCharArray(L"USB \u9a71\u52a8\u5df2\u66f4\u65b0\u5e76\u91cd\u65b0\u8fde\u63a5\u3002"));
    } else {
        AppMessageDialog::warning(
            this,
            QString::fromWCharArray(L"USB \u9a71\u52a8\u66f4\u65b0\u5f85\u786e\u8ba4"),
            reconnected
                ? QString::fromWCharArray(L"USB \u8bbe\u5907\u5df2\u91cd\u65b0\u8fde\u63a5\uff0c\u4f46\u7248\u672c\u6821\u9a8c\u672a\u901a\u8fc7\u3002\u8bf7\u91cd\u8bd5\u66f4\u65b0\u3002")
                : QString::fromWCharArray(L"USB \u9a71\u52a8\u5df2\u5199\u5165\uff0c\u4f46\u6682\u672a\u7b49\u5230\u8bbe\u5907\u91cd\u65b0\u8fde\u63a5\u3002\u8bf7\u91cd\u63d2 USB \u8bbe\u5907\u3002"));
    }
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
        if (suppressAutoPlay_ && kQueryCancelAutoPlayMessage != 0 &&
            msg->message == kQueryCancelAutoPlayMessage) {
            if (result) *result = TRUE;
            return true;
        }
        if (msg->message == WM_INPUT) {
            rawInput_.handleNativeMessage(msg, result);
        }
        switch (msg->message) {
        case WM_DEVICECHANGE:
            if (!suppressAutoPlay_ &&
                isXiaoJiUsbDeviceChange(msg->wParam, msg->lParam)) {
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
    statusOverlay_.updateStatus(config_, profile, macro, repeatRunning, macroRunning);
    view_.updateActivityIcons(this, repeatRunning, macroRunning);
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

    std::wstringstream errorLog;
    errorLog << L"driver_mode=";
    if (config_.driverMode == DriverMode::Usb) errorLog << L"USB";
    else if (config_.driverMode == DriverMode::Native) errorLog << L"XiaoJiVhid";
    else errorLog << (config_.driverMode == DriverMode::Hid ? L"HID" : L"DD");
    errorLog << L"\n"
             << L"last_error=" << (activeInputDriverError().empty() ? L"(none)" : activeInputDriverError()) << L"\n"
             << L"status_note=" << (driverStatusNote_.empty() ? L"(none)" : driverStatusNote_) << L"\n";
    ddRuntimeService_.LogRuntimeError(L"dd-runtime.log", errorLog.str());
}
