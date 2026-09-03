#include "MainWindow.h"

#include "dialogs/AppMessageDialog.h"
#include "dialogs/ConfigConfirmDialog.h"
#include "dialogs/DialogChrome.h"
#include "services/AgentService.h"
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
    inputRecoveryTimer_.setSingleShot(true);
    inputRecoveryTimer_.setInterval(700);
    connect(&inputRecoveryTimer_, &QTimer::timeout, this, [this]() {
        recoverInputAfterResume();
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
    engineStateCoordinator_.stopThreads();
    inputDriver_.ReleaseAll();
    inputDriver_.ClearAllStates();
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
    configCoordinator_.load();
    config_.backgroundAutoRun = false;
    configCoordinator_.save(false, 500);
    initializeAgentHandoff();
    soundService_.warmUpToggleSounds(
        config_.customSoundOn,
        config_.customSoundOff,
        config_.macroSoundOn,
        config_.macroSoundOff);
    // Keep the selected HID mode on startup. A transient HID failure must not
    // silently switch the user's configuration to legacy DD.
    if (!loadConfiguredDriver(false)) {
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
    inputDriver_.ReleaseAll();
    inputDriver_.ClearAllStates();
    inputDriver_.SetDriver(nullptr);
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
    inputDriver_.ReleaseAll();
    inputDriver_.ClearAllStates();
    inputDriver_.SetDriver(nullptr);
    ch552Driver_.Disconnect();
    driver_.ReleaseAll();
    driver_.Unload();
    if (!ensureLegacyDdInfInstalled()) {
        return false;
    }

    bool loaded = driver_.LoadFromDirectory(appPaths_.exeDir(), false);
    if (!loaded) {
        // Resume can leave the INF services reporting ready while the runtime
        // handles are still stale. The repair helper probes the runtime first,
        // then refreshes or rebuilds the device instances only when needed.
        std::wstring repairMessage;
        loaded = ddRuntimeService_.RepairEmbeddedInfDriver(
            [this]() {
                return driver_.LoadFromDirectory(appPaths_.exeDir(), false);
            },
            &repairMessage);
        if (!loaded && !repairMessage.empty()) {
            driverStatusNote_ = repairMessage;
        }
    }
    if (loaded) {
        inputDriver_.SetDriver(&driver_);
    }
    return loaded;
}

bool MainWindow::loadCh552UsbDriver() {
    inputDriver_.ReleaseAll();
    inputDriver_.ClearAllStates();
    inputDriver_.SetDriver(nullptr);
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

bool MainWindow::initializeAgentHandoff() {
    // USB background Agent is temporarily disabled; remove any previous installation.
    config_.backgroundAutoRun = false;
    AgentService::Uninstall(nullptr);
    return true;
}

void MainWindow::setBackgroundAutoRun(bool enabled) {
    Q_UNUSED(enabled);
    config_.backgroundAutoRun = false;
    configCoordinator_.save(false, 500);
    initializeAgentHandoff();
    if (auto* page = view_.driverPage()) {
        page->setBackgroundAutoRun(false);
        page->setBackgroundAutoRunAvailable(false);
    }
}

void MainWindow::repairAgent() {
    initializeAgentHandoff();
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

    if (targetDriver.LoadFromDirectory(appPaths_.exeDir(), false)) {
        return true;
    }

    std::wstring repairMessage;
    if (ddRuntimeService_.RepairEmbeddedInfDriver(
            [&targetDriver, this]() {
                return targetDriver.LoadFromDirectory(appPaths_.exeDir(), false);
            },
            &repairMessage)) {
        return true;
    }

    if (errorMessage) {
        *errorMessage = targetDriver.GetLastError();
        if (!repairMessage.empty()) {
            if (!errorMessage->empty()) *errorMessage += L"; ";
            *errorMessage += L"INF repair=" + repairMessage;
        }
    }
    targetDriver.ReleaseAll();
    targetDriver.Unload();
    return false;
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
    page->setBackgroundAutoRun(false);
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
    page->setDriverActions(ddUninstallAvailable, hidUninstallAvailable, isUsbDriverUpdateAvailable());

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
    page->setBackgroundAutoRunAvailable(false);
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

bool MainWindow::isUsbDriverUpdateAvailable() const {
    // Firmware rollout is paused until the current CH552 release is verified.
    return false;
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
        inputDriver_.SetDriver(nullptr);
        driver_.ReleaseAll();
        driver_.Unload();
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

void MainWindow::updateUsbDriver(bool requireConfirmation) {
    // Keep the old updater implementation available for a later release, but do not expose it now.
    Q_UNUSED(requireConfirmation);
    return;

    QFile firmware(kEmbeddedUsbFirmwareResource);
    QFile updater(kEmbeddedUsbUpdaterResource);
    QFile winUsbInstaller(QStringLiteral(":/firmware/updater/xiaoji-winusb-installer.exe"));
    if (!firmware.exists() || !updater.exists() || !winUsbInstaller.exists()) {
        AppMessageDialog::warning(
            this,
            QString::fromWCharArray(L"USB \u56fa\u4ef6\u66f4\u65b0\u5931\u8d25"),
            QString::fromWCharArray(L"\u7a0b\u5e8f\u5185\u7f6e\u7684 USB \u56fa\u4ef6\u66f4\u65b0\u6587\u4ef6\u4e0d\u5b58\u5728\u3002"));
        return;
    }

    if (requireConfirmation) {
        const bool confirmed = ConfigConfirmDialog::confirm(
            this,
            QString::fromWCharArray(L"\u66f4\u65b0 USB \u56fa\u4ef6"),
            QString::fromWCharArray(L"\u5c06\u81ea\u52a8\u66f4\u65b0 USB \u8bbe\u5907\u56fa\u4ef6\u3002\u66f4\u65b0\u8fc7\u7a0b\u4e2d\u8bf7\u4e0d\u8981\u62d4\u51fa USB\u3002\u662f\u5426\u7ee7\u7eed\uff1f"),
            QString::fromWCharArray(L"\u66f4\u65b0"));
        if (!confirmed) {
            return;
        }
    }

    inputHook_.setDialogOpen(true);
    suppressAutoPlay_ = true;
    auto* progressDialog = new DriverProgressDialog(
        QString::fromWCharArray(L"\u66f4\u65b0 USB \u56fa\u4ef6"),
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
            QString::fromWCharArray(L"USB \u56fa\u4ef6\u66f4\u65b0\u5931\u8d25"),
            QString::fromWCharArray(L"\u672a\u68c0\u6d4b\u5230\u53ef\u7528\u7684 USB \u8bbe\u5907\u3002\u8bf7\u786e\u8ba4\u8bbe\u5907\u5df2\u76f4\u8fde\u7535\u8111 USB \u63a5\u53e3\uff0c\u7136\u540e\u91cd\u8bd5\u3002"));
        refreshDriverStatusUi();
        return;
    }

    const std::wstring sourceFirmwareVersion = ch552Driver_.FirmwareVersion();
    const std::wstring sourcePortName = ch552Driver_.PortName();

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
            QString::fromWCharArray(L"USB \u56fa\u4ef6\u66f4\u65b0\u5931\u8d25"),
            preparationError.isEmpty()
                ? QString::fromWCharArray(L"\u65e0\u6cd5\u51c6\u5907 USB \u56fa\u4ef6\u66f4\u65b0\u6587\u4ef6\u3002")
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
            QString::fromWCharArray(L"USB \u56fa\u4ef6\u66f4\u65b0\u5931\u8d25"),
            QString::fromWCharArray(L"\u56fa\u4ef6\u66f4\u65b0\u6240\u9700\u7684 Windows USB \u7ec4\u4ef6\u5b89\u88c5\u672a\u5b8c\u6210\u3002\u8bf7\u5141\u8bb8\u7ba1\u7406\u5458\u6388\u6743\u540e\u91cd\u8bd5\u3002\n\n") + winUsbError);
        return;
    }

    const auto updateRequestResult = ch552Driver_.EnterBootloaderForUpdate();
    if (updateRequestResult == Ch552UsbDriver::UpdateRequestResult::Failed) {
        const QString error = QString::fromStdWString(ch552Driver_.GetLastError());
        ddRuntimeService_.LogRuntimeError(
            L"usb-update.log",
            L"Firmware update request failed. source_firmware=" + sourceFirmwareVersion +
                L", source_port=" + sourcePortName +
                L", error=" + error.toStdWString());
        progressDialog->accept();
        progressDialog->deleteLater();
        inputHook_.setDialogOpen(false);
        suppressAutoPlay_ = false;
        AppMessageDialog::warning(
            this,
            QString::fromWCharArray(L"USB \u56fa\u4ef6\u66f4\u65b0\u5931\u8d25"),
            QString::fromWCharArray(
                L"USB \u8bbe\u5907\u672a\u63a5\u53d7\u56fa\u4ef6\u66f4\u65b0\u8bf7\u6c42\u3002\u8bf7\u5b8c\u5168\u62d4\u51fa\u8bbe\u5907\uff0c\u7b49\u5f85 5 \u79d2\u540e\u76f4\u8fde\u7535\u8111 USB \u63a5\u53e3\u91cd\u8bd5\u3002\n\n"
                L"\u8be6\u7ec6\u4fe1\u606f\u5df2\u5199\u5165 logs \u76ee\u5f55\u3002"));
        refreshDriverStatusUi();
        return;
    }
    const bool updateRequestAcknowledged =
        updateRequestResult == Ch552UsbDriver::UpdateRequestResult::Acknowledged;

    inputDriver_.SetDriver(nullptr);
    progressDialog->setMessage(QString::fromWCharArray(L"\u6b63\u5728\u7b49\u5f85 USB \u8fdb\u5165\u66f4\u65b0\u6a21\u5f0f..."));
    QElapsedTimer bootloaderTimer;
    bootloaderTimer.start();
    bool bootloaderPresent = false;
    while (bootloaderTimer.elapsed() < 10000) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
        if (isUsbDevicePresent(kCh552BootloaderVidPid)) {
            bootloaderPresent = true;
            break;
        }
        QThread::msleep(100);
    }
    if (!bootloaderPresent) {
        ddRuntimeService_.LogRuntimeError(
            L"usb-update.log",
            L"Firmware update mode was not detected after the request was sent.\n"
                L"request_acknowledged=" +
                (updateRequestAcknowledged ? std::wstring(L"1") : std::wstring(L"0")) +
                L", source_firmware=" + sourceFirmwareVersion +
                L", source_port=" + sourcePortName +
                L", normal_device_present=" +
                (isUsbDevicePresent(kXiaoJiUsbVidPid) ? std::wstring(L"1") : std::wstring(L"0")));
        progressDialog->accept();
        progressDialog->deleteLater();
        inputHook_.setDialogOpen(false);
        suppressAutoPlay_ = false;
        engineHookCoordinator_.refreshState();
        refreshDriverStatusUi();
        AppMessageDialog::warning(
            this,
            QString::fromWCharArray(L"USB \u56fa\u4ef6\u66f4\u65b0\u5931\u8d25"),
            QString::fromWCharArray(
                L"\u56fa\u4ef6\u66f4\u65b0\u8bf7\u6c42\u5df2\u53d1\u9001\uff0c\u4f46 Windows \u672a\u80fd\u8bc6\u522b\u8bbe\u5907\u7684\u66f4\u65b0\u6a21\u5f0f\u3002\n\n"
                L"\u8bf7\u5b8c\u5168\u62d4\u51fa\u8bbe\u5907\uff0c\u7b49\u5f85 5 \u79d2\u540e\u76f4\u8fde\u7535\u8111 USB \u63a5\u53e3\u91cd\u8bd5\uff0c\u4e0d\u8981\u4f7f\u7528 USB \u6269\u5c55\u575e\u6216\u96c6\u7ebf\u5668\u3002\n\n"
                L"\u5982\u679c\u4ecd\u7136\u5931\u8d25\uff0c\u8bf7\u63d0\u4f9b logs \u76ee\u5f55\u4e2d\u7684 usb-update.log \u548c winusb-install.log\u3002"));
        return;
    }

    progressDialog->setMessage(QString::fromWCharArray(L"\u6b63\u5728\u5199\u5165 USB \u8bbe\u5907\u56fa\u4ef6..."));
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
        const bool bootloaderStillPresent = isUsbDevicePresent(kCh552BootloaderVidPid);
        ddRuntimeService_.LogRuntimeError(
            L"usb-update.log",
            L"USB updater failed. started=" + std::to_wstring(updaterStarted) +
                L", exit_code=" + std::to_wstring(updateProcess.exitCode()) +
                L", bootloader_present=" + (bootloaderStillPresent ? std::wstring(L"1") : std::wstring(L"0")) +
                L", request_acknowledged=" +
                (updateRequestAcknowledged ? std::wstring(L"1") : std::wstring(L"0")) +
                L", source_firmware=" + sourceFirmwareVersion +
                L", source_port=" + sourcePortName + L"\n" +
                (updateOutput.isEmpty() ? std::wstring(L"(no output)") : updateOutput.toStdWString()));
        progressDialog->accept();
        progressDialog->deleteLater();
        inputHook_.setDialogOpen(false);
        suppressAutoPlay_ = false;
        engineHookCoordinator_.refreshState();
        refreshDriverStatusUi();
        AppMessageDialog::warning(
            this,
            QString::fromWCharArray(L"USB \u56fa\u4ef6\u66f4\u65b0\u5931\u8d25"),
            bootloaderStillPresent
                ? QString::fromWCharArray(L"\u8bbe\u5907\u5df2\u8fdb\u5165\u56fa\u4ef6\u66f4\u65b0\u6a21\u5f0f\uff0c\u4f46\u56fa\u4ef6\u5199\u5165\u672a\u5b8c\u6210\u3002\u8bf7\u62d4\u51fa\u5e76\u91cd\u65b0\u63d2\u5165\u8bbe\u5907\u540e\u91cd\u8bd5\u3002\n\n\u5982\u679c\u4ecd\u7136\u5931\u8d25\uff0c\u8bf7\u63d0\u4f9b logs \u76ee\u5f55\u4e2d\u7684 usb-update.log \u548c winusb-install.log\u3002")
                : QString::fromWCharArray(L"\u56fa\u4ef6\u5199\u5165\u8fc7\u7a0b\u4e2d USB \u8bbe\u5907\u5df2\u65ad\u5f00\u3002\u8bf7\u5b8c\u5168\u62d4\u51fa\u8bbe\u5907\uff0c\u7b49\u5f85 5 \u79d2\u540e\u76f4\u8fde\u7535\u8111 USB \u63a5\u53e3\u91cd\u8bd5\u3002\n\n\u5982\u679c\u4ecd\u7136\u5931\u8d25\uff0c\u8bf7\u63d0\u4f9b logs \u76ee\u5f55\u4e2d\u7684 usb-update.log \u548c winusb-install.log\u3002"));
        return;
    }

    progressDialog->markFinished(QString::fromWCharArray(L"USB \u56fa\u4ef6\u5df2\u5199\u5165\uff0c\u6b63\u5728\u7b49\u5f85\u8bbe\u5907\u91cd\u65b0\u8fde\u63a5..."));
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
            QString::fromWCharArray(L"USB \u56fa\u4ef6\u66f4\u65b0\u5b8c\u6210"),
            QString::fromWCharArray(L"USB \u8bbe\u5907\u56fa\u4ef6\u5df2\u66f4\u65b0\u5e76\u91cd\u65b0\u8fde\u63a5\u3002"));
    } else {
        AppMessageDialog::warning(
            this,
            QString::fromWCharArray(L"USB \u56fa\u4ef6\u66f4\u65b0\u5f85\u786e\u8ba4"),
            reconnected
                ? QString::fromWCharArray(L"USB \u8bbe\u5907\u5df2\u91cd\u65b0\u8fde\u63a5\uff0c\u4f46\u56fa\u4ef6\u7248\u672c\u6821\u9a8c\u672a\u901a\u8fc7\u3002\u8bf7\u91cd\u8bd5\u66f4\u65b0\u3002")
                : QString::fromWCharArray(L"USB \u56fa\u4ef6\u5df2\u5199\u5165\uff0c\u4f46\u6682\u672a\u7b49\u5230\u8bbe\u5907\u91cd\u65b0\u8fde\u63a5\u3002\u8bf7\u91cd\u63d2 USB \u8bbe\u5907\u3002"));
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
            if (!suppressAutoPlay_ &&
                isXiaoJiUsbDeviceChange(msg->wParam, msg->lParam)) {
                usbDeviceRefreshTimer_.start(650);
            }
            break;
        case WM_INPUT_DEVICE_CHANGE:
            rawInput_.clearDeviceCache();
            if (config_.driverMode == DriverMode::Hid && msg->wParam == GIDC_REMOVAL) {
                // A device-change notification can arrive after Windows has
                // invalidated the old Raw Input registration. Drop it now so
                // the low-level hook remains available during recovery.
                rawInput_.unregisterWindow();
                engineHookCoordinator_.refreshState();
                inputRecoveryAttempts_ = 0;
                inputRecoveryTimer_.start();
            } else {
                engineHookCoordinator_.refreshState();
            }
            break;
        case WM_POWERBROADCAST:
            switch (msg->wParam) {
            case PBT_APMSUSPEND:
            case PBT_APMSTANDBY:
                // Do not carry stale HID handles or pressed-key state across
                // a suspend. The runtime is rebuilt after Windows resumes.
                engineStateCoordinator_.setRepeatRunning(false);
                engine_.ReleaseAll();
                macroEngine_.StopAll();
                inputDriver_.ReleaseAll();
                inputDriver_.ClearAllStates();
                inputDriver_.SetDriver(nullptr);
                inputHook_.stop();
                rawInput_.unregisterWindow();
                driver_.ReleaseAll();
                driver_.Unload();
                ch552Driver_.Disconnect();
                break;
            case PBT_APMRESUMEAUTOMATIC:
            case PBT_APMRESUMESUSPEND:
            case PBT_APMRESUMECRITICAL:
                inputRecoveryAttempts_ = 0;
                inputRecoveryTimer_.start();
                break;
            default:
                break;
            }
            if (result) *result = TRUE;
            return true;
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

void MainWindow::recoverInputAfterResume() {
    if (systemShutdownRequested_) return;

    constexpr int kMaxInputRecoveryAttempts = 5;
    ++inputRecoveryAttempts_;

    // Sleep can drop the selected HID device and leave injected keys held.
    // Tear down the complete input pipeline before probing the runtime again.
    engineStateCoordinator_.setRepeatRunning(false);
    engine_.ReleaseAll();
    macroEngine_.StopAll();
    inputDriver_.ReleaseAll();
    inputDriver_.ClearAllStates();
    inputDriver_.SetDriver(nullptr);

    inputHook_.stop();
    rawInput_.unregisterWindow();

    // A suspended HID instance can disappear while the service, SYS and DLL
    // are still present. Do not immediately fall back to DD here: that path
    // changes the saved mode and can leave both runtimes unavailable. Keep the
    // user's HID choice and probe it again on the next recovery pass if
    // Windows is still bringing the device back.
    bool driverLoaded = loadConfiguredDriver(false);
    if (!driverLoaded && config_.driverMode == DriverMode::Hid && inputRecoveryAttempts_ <= 2) {
        std::wstring repairMessage;
        const bool repairAccepted = ddRuntimeService_.InstallEmbeddedHidDriver(&repairMessage);
        if (!repairMessage.empty()) {
            ddRuntimeService_.LogRuntimeError(
                L"dd-runtime.log",
                L"HID resume repair attempt=" + std::to_wstring(inputRecoveryAttempts_) +
                    L" accepted=" + (repairAccepted ? std::wstring(L"1") : std::wstring(L"0")) +
                    L"\n" + repairMessage);
        }
        driverLoaded = loadConfiguredDriver(false);
    }
    if (driverLoaded) {
        driverUnavailableLogged_ = false;
    }

    bool rawInputReady = false;
    if (driverLoaded) {
        rawInputReady = rawInput_.registerWindow(reinterpret_cast<HWND>(winId()));
        if (!rawInputReady) {
            // A partial RegisterRawInputDevices result must not leave the
            // low-level hook suppressed for only one device class.
            rawInput_.unregisterWindow();
        }
    } else {
        // Never suppress the low-level hook while the selected driver is down.
        rawInput_.unregisterWindow();
    }
    engineHookCoordinator_.refreshState();
    const bool hookReady = inputHook_.start();

    if (!driverLoaded || !rawInputReady || !hookReady) {
        if (inputRecoveryAttempts_ < kMaxInputRecoveryAttempts) {
            driverStatusNote_ = !driverLoaded
                ? L"\u4f11\u7720\u6062\u590d\u540e\u8f93\u5165\u9a71\u52a8\u4e0d\u53ef\u7528\uff0c\u6b63\u5728\u91cd\u8bd5\u3002"
                : L"\u4f11\u7720\u6062\u590d\u540e\u8f93\u5165\u94fe\u8def\u672a\u5b8c\u6210\u6062\u590d\uff0c\u6b63\u5728\u91cd\u8bd5\u3002";
            inputRecoveryTimer_.start(1000);
        } else {
            driverStatusNote_ = !driverLoaded
                ? L"\u4f11\u7720\u6062\u590d\u540e\u8f93\u5165\u9a71\u52a8\u4e0d\u53ef\u7528\u3002"
                : L"\u4f11\u7720\u6062\u590d\u540e\u8f93\u5165\u94fe\u8def\u6062\u590d\u5931\u8d25\u3002";
            logDriverUnavailableOnce();
        }
    } else {
        inputRecoveryAttempts_ = 0;
        if (!driverStatusFallback_) {
            driverStatusNote_.clear();
        }
    }

    refreshDriverStatusUi();
    refreshStatusOverlay();
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
             << L"recovery_attempts=" << inputRecoveryAttempts_ << L"\n"
             << L"status_note=" << (driverStatusNote_.empty() ? L"(none)" : driverStatusNote_) << L"\n";
    ddRuntimeService_.LogRuntimeError(L"dd-runtime.log", errorLog.str());
}
