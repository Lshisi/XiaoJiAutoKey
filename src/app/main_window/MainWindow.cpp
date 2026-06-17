#include "MainWindow.h"

#include "dialogs/AppMessageDialog.h"
#include "dialogs/ConfigConfirmDialog.h"
#include "ui/pages/DriverPage.h"
#include "ui/pages/SettingsPage.h"
#include "ui/styles/DesignTokens.h"

#include <QAbstractButton>
#include <QApplication>
#include <QCloseEvent>
#include <QDialog>
#include <QEvent>
#include <QFont>
#include <QLabel>
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
#include <memory>
#include <stdexcept>
#include <sstream>

namespace {
constexpr int kBaseWindowWidth = 780;
constexpr int kBaseWindowHeight = 560;
constexpr int kBaseDpi = 96;

int dpiScaled(int value, int dpi) {
    return qMax(value, (value * dpi + kBaseDpi / 2) / kBaseDpi);
}

int dpiFromWParam(WPARAM value) {
    const int dpiX = LOWORD(value);
    const int dpiY = HIWORD(value);
    return qMax(kBaseDpi, dpiX > 0 ? dpiX : dpiY);
}

#ifdef Q_OS_WIN
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
        setModal(true);
        setWindowModality(Qt::ApplicationModal);
        setFixedSize(420, 168);

        auto* root = new QVBoxLayout(this);
        root->setContentsMargins(24, 20, 24, 20);
        root->setSpacing(12);

        auto* titleLabel = new QLabel(title, this);
        titleLabel->setFont(Fluent::Typography::cardTitle());
        root->addWidget(titleLabel);

        messageLabel_ = new QLabel(message, this);
        messageLabel_->setFont(Fluent::Typography::body());
        messageLabel_->setWordWrap(true);
        root->addWidget(messageLabel_);

        progressBar_ = new QProgressBar(this);
        progressBar_->setRange(0, 100);
        progressBar_->setValue(8);
        progressBar_->setTextVisible(true);
        progressBar_->setFixedHeight(22);
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

protected:
    void closeEvent(QCloseEvent* event) override {
        if (event) event->ignore();
    }

    void reject() override {}

private:
    QLabel* messageLabel_ = nullptr;
    QProgressBar* progressBar_ = nullptr;
    QTimer timer_;
};

}

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent),
      appPaths_(),
      configCoordinator_(appPaths_.configPath(), this),
      profileController_(config_),
      macroController_(config_),
      engine_(driver_),
      macroEngine_(driver_),
      inputHook_(this),
      ddRuntimeService_(std::filesystem::path(appPaths_.exeDir()), appPaths_.logsDir()),
      soundService_(appPaths_.exeDir()),
      statusOverlay_(nullptr) {
    runtimeVisualRefreshTimer_.setSingleShot(true);
    connect(&runtimeVisualRefreshTimer_, &QTimer::timeout, this, [this]() {
        applyRuntimeVisualRefresh();
    });
}

MainWindow::~MainWindow() {
    engineStateCoordinator_.stopThreads();
}

bool MainWindow::Initialize() {
    setupUi();
    configCoordinator_.load();
    soundService_.warmUpToggleSounds(
        config_.customSoundOn,
        config_.customSoundOff,
        config_.macroSoundOn,
        config_.macroSoundOff);
    if (!loadConfiguredDriver()) {
        std::wstringstream errorLog;
        errorLog << L"driver_mode=" << (config_.driverMode == DriverMode::Hid ? L"HID" : L"DD") << L"\n"
                 << L"last_error=" << (driver_.GetLastError().empty() ? L"(none)" : driver_.GetLastError()) << L"\n"
                 << L"status_note=" << (driverStatusNote_.empty() ? L"(none)" : driverStatusNote_) << L"\n";
        ddRuntimeService_.LogRuntimeError(L"dd-runtime.log", errorLog.str());
    }
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
    refreshDriverStatusUi();
    refreshStatusOverlay();
    bindDpiRefreshSignals();
    observedScreenDpi_ = currentScreenDpi();
    QTimer::singleShot(250, this, [this]() {
        view_.initializeTray(this);
    });
    return true;
}

bool MainWindow::loadConfiguredDriver() {
    driverStatusNote_.clear();
    if (config_.driverMode == DriverMode::Hid) {
        if (loadHidDriver()) {
            return true;
        }
        driverStatusNote_ = L"HID \u52a0\u8f7d\u5931\u8d25\uff0c\u5df2\u56de\u9000\u65e7 DD: " +
            (driver_.GetLastError().empty() ? std::wstring(L"(none)") : driver_.GetLastError());
        const bool legacyLoaded = loadLegacyDriver();
        if (legacyLoaded) {
            config_.driverMode = DriverMode::Legacy;
            configCoordinator_.save();
        }
        return legacyLoaded;
    }
    return loadLegacyDriver();
}

bool MainWindow::loadHidDriver() {
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
    driver_.ReleaseAll();
    driver_.Unload();
    ensureLegacyDdInfInstalled();
    return driver_.LoadFromDirectory(appPaths_.exeDir(), false);
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

    QString runtimeText = QString::fromWCharArray(L"\u672a\u52a0\u8f7d");
    if (driver_.IsReady()) {
        runtimeText = driver_.GetRuntimeKind() == DdDriver::RuntimeKind::Hid
            ? QStringLiteral("HID")
            : QStringLiteral("DD");
    }
    page->setCurrentRuntime(runtimeText);

    QString summary = driver_.IsReady()
        ? QString::fromWCharArray(L"\u53ef\u7528")
        : QString::fromWCharArray(L"\u4e0d\u53ef\u7528");
    if (!driverStatusNote_.empty()) {
        summary += QString::fromWCharArray(L" / \u5df2\u56de\u9000");
    }
    page->setStatusSummary(summary);
    const bool hidUninstallAvailable =
        status.hidServiceInstalled ||
        status.hidServicePendingDelete ||
        status.hidSysInstalled ||
        status.hidDriverStorePresent ||
        status.hidRootDeviceOk ||
        status.hidChildDeviceOk;
    page->setHidUninstallAvailable(hidUninstallAvailable);

    std::wstringstream detail;
    detail << L"\u914d\u7f6e\u9a71\u52a8: " << (config_.driverMode == DriverMode::Hid ? L"HID" : L"DD") << L"\n";
    detail << L"\u5f53\u524d\u8fd0\u884c: ";
    if (!driver_.IsReady()) detail << L"\u672a\u52a0\u8f7d";
    else detail << (driver_.GetRuntimeKind() == DdDriver::RuntimeKind::Hid ? L"HID" : L"DD");
    detail << L"\n\n";
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

    if (mode == DriverMode::Hid) {
        inputHook_.setDialogOpen(true);
        const bool confirmed = ConfigConfirmDialog::confirm(
            this,
            QString::fromWCharArray(L"\u9a71\u52a8\u5207\u6362"),
            QString::fromWCharArray(L"\u5207\u6362\u5230 HID \u9700\u8981\u91cd\u542f\u7a0b\u5e8f\uff0c\u70b9\u51fb\u786e\u5b9a\u540e\u5c06\u81ea\u52a8\u91cd\u65b0\u542f\u52a8\u3002"),
            QString::fromWCharArray(L"\u786e\u5b9a"));
        inputHook_.setDialogOpen(false);
        if (!confirmed) {
            refreshDriverStatusUi();
            return DriverCoordinator::SwitchResult::Canceled;
        }
        if (!ensureHidDriverInstalledForSwitch()) {
            refreshDriverStatusUi();
            return DriverCoordinator::SwitchResult::Canceled;
        }
    }

    engineStateCoordinator_.setRepeatRunning(false);
    macroEngine_.StopAll();
    driver_.ReleaseAll();
    driver_.Unload();
    driverStatusNote_.clear();

    config_.driverMode = mode;
    const bool loaded = (mode == DriverMode::Hid)
        ? restartForDriverModeChange()
        : loadConfiguredDriver();
    if (!loaded) {
        config_.driverMode = originalMode;
        configCoordinator_.save();
        loadConfiguredDriver();
    } else {
        configCoordinator_.save();
    }
    refreshDriverStatusUi();
    return loaded ? DriverCoordinator::SwitchResult::Succeeded : DriverCoordinator::SwitchResult::Failed;
}

bool MainWindow::restartForDriverModeChange() {
    configCoordinator_.save();
    suppressDdUnavailablePrompt_ = true;

    const QString program = QCoreApplication::applicationFilePath();
    const QStringList arguments = { QStringLiteral("--restart-wait") };
    if (!QProcess::startDetached(program, arguments, QCoreApplication::applicationDirPath())) {
        suppressDdUnavailablePrompt_ = false;
        driverStatusNote_ = L"\u5207\u6362\u5230 HID \u9700\u8981\u91cd\u542f\uff0c\u4f46\u65b0\u8fdb\u7a0b\u542f\u52a8\u5931\u8d25\u3002";
        return false;
    }

    QCoreApplication::quit();
    return true;
}

void MainWindow::prepareHidDriverUninstall() {
    if (driver_.GetRuntimeKind() == DdDriver::RuntimeKind::Hid) {
        engineStateCoordinator_.setRepeatRunning(false);
        macroEngine_.StopAll();
        driver_.ReleaseAll();
        driver_.Unload();
    }
}

void MainWindow::finalizeHidDriverUninstall() {
    if (config_.driverMode == DriverMode::Hid) {
        config_.driverMode = DriverMode::Legacy;
        configCoordinator_.save();
        loadLegacyDriver();
    }
    refreshDriverStatusUi();
}

bool MainWindow::rebootComputer() {
    const QString program = QStringLiteral("shutdown.exe");
    const QStringList arguments = {
        QStringLiteral("/r"),
        QStringLiteral("/t"), QStringLiteral("8"),
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
    const auto* msg = static_cast<MSG*>(message);
    if (msg) {
        switch (msg->message) {
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
    if (suppressDdUnavailablePrompt_ || ddUnavailablePromptVisible_) {
        return;
    }
    ddUnavailablePromptVisible_ = true;

    std::wstringstream errorLog;
    errorLog << L"driver_mode=" << (config_.driverMode == DriverMode::Hid ? L"HID" : L"DD") << L"\n"
             << L"last_error=" << (driver_.GetLastError().empty() ? L"(none)" : driver_.GetLastError()) << L"\n"
             << L"status_note=" << (driverStatusNote_.empty() ? L"(none)" : driverStatusNote_) << L"\n";
    ddRuntimeService_.LogRuntimeError(L"dd-runtime.log", errorLog.str());

    AppMessageDialog::warning(
        this,
        QString::fromWCharArray(L"\u9a71\u52a8\u52a0\u8f7d\u5931\u8d25"),
        QString::fromWCharArray(L"\u5f53\u524d\u9a71\u52a8\u4e0d\u53ef\u7528\uff0c\u65e0\u6cd5\u6a21\u62df\u6309\u952e\u6216\u9f20\u6807\u3002\n\n\u8be6\u7ec6\u9519\u8bef\u5df2\u5199\u5165 logs \u76ee\u5f55\uff0c\u8bf7\u68c0\u67e5 dd-runtime.log\u3002"));

    ddUnavailablePromptVisible_ = false;
}
