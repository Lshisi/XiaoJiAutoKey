#include "MainWindow.h"
#include "ConfigStore.h"
#include "DdDriver.h"
#include "services/AppPaths.h"
#include "services/CodeIntegrityInspector.h"
#include "services/DdRuntimeService.h"
#include "services/UpdateService.h"
#include "ui/styles/DesignTokens.h"

#include <QApplication>
#include <QKeyEvent>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QTextEdit>
#include <QCoreApplication>
#include <QFont>
#include <QGuiApplication>
#include <QObject>
#include <QStringList>
#include <QStyleFactory>
#include <QTimer>
#include <QQuickStyle>
#include <QQuickWindow>

#include <Windows.h>
#include <mmsystem.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#ifndef PROCESS_POWER_THROTTLING_IGNORE_TIMER_RESOLUTION
#define PROCESS_POWER_THROTTLING_IGNORE_TIMER_RESOLUTION 0x4
#endif

#pragma comment(lib, "winmm.lib")

namespace {

class SpaceInputFilter final : public QObject {
public:
    using QObject::QObject;

protected:
    bool eventFilter(QObject* watched, QEvent* event) override {
        if (event->type() == QEvent::KeyPress || event->type() == QEvent::KeyRelease
            || event->type() == QEvent::ShortcutOverride) {
            const auto* keyEvent = static_cast<QKeyEvent*>(event);
            if (keyEvent->key() == Qt::Key_Space) {
                // Claim Space before Qt resolves shortcuts, even when entering text.
                if (event->type() != QEvent::ShortcutOverride &&
                    (isEditableTextTarget(watched) || isEditableTextTarget(QGuiApplication::focusObject()))) {
                    return QObject::eventFilter(watched, event);
                }
                event->accept();
                return true;
            }
        }
        return QObject::eventFilter(watched, event);
    }

private:
    static bool isEditableTextTarget(QObject* target) {
        if (!target) return false;
        if (target->inherits("QQuickTextInput") || target->inherits("QQuickTextEdit")) {
            return !target->property("readOnly").toBool();
        }
        if (auto* edit = qobject_cast<QLineEdit*>(target)) {
            return edit->isEnabled() && !edit->isReadOnly();
        }
        if (auto* edit = qobject_cast<QTextEdit*>(target)) {
            return edit->isEnabled() && !edit->isReadOnly();
        }
        if (auto* edit = qobject_cast<QPlainTextEdit*>(target)) {
            return edit->isEnabled() && !edit->isReadOnly();
        }
        // Multiline editors may receive key events through their viewport.
        if (auto* edit = qobject_cast<QTextEdit*>(target->parent())) {
            return target == edit->viewport() && edit->isEnabled() && !edit->isReadOnly();
        }
        if (auto* edit = qobject_cast<QPlainTextEdit*>(target->parent())) {
            return target == edit->viewport() && edit->isEnabled() && !edit->isReadOnly();
        }
        return false;
    }
};

void PreserveTimerResolution() {
    PROCESS_POWER_THROTTLING_STATE state{};
    state.Version = PROCESS_POWER_THROTTLING_CURRENT_VERSION;
    state.ControlMask = PROCESS_POWER_THROTTLING_IGNORE_TIMER_RESOLUTION;
    state.StateMask = 0;
    ::SetProcessInformation(
        ::GetCurrentProcess(),
        ProcessPowerThrottling,
        &state,
        static_cast<DWORD>(sizeof(state)));
}

const char* RuntimeKindText(DdDriver::RuntimeKind kind) {
    switch (kind) {
    case DdDriver::RuntimeKind::Hid:
        return "hid";
    case DdDriver::RuntimeKind::Legacy:
        return "legacy";
    default:
        return "unknown";
    }
}

const char* DriverModeText(DriverMode mode) {
    switch (mode) {
    case DriverMode::Hid:
        return "hid";
    case DriverMode::Usb:
        return "usb";
    default:
        return "legacy";
    }
}

std::string ToUtf8Line(const std::wstring& text) {
    QString value = QString::fromStdWString(text);
    value.replace('\r', QStringLiteral("\\r"));
    value.replace('\n', QStringLiteral("\\n"));
    return value.toUtf8().constData();
}

std::string ReadFileText(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return {};
    }
    std::ostringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

std::string DetectDriverModeFromRawConfig(const std::string& text) {
    if (text.find("\"driverMode\": \"hid\"") != std::string::npos ||
        text.find("\"driverMode\":\"hid\"") != std::string::npos) {
        return "hid";
    }
    if (text.find("\"driverMode\": \"legacy\"") != std::string::npos ||
        text.find("\"driverMode\":\"legacy\"") != std::string::npos) {
        return "legacy";
    }
    if (text.find("\"driverMode\": \"usb\"") != std::string::npos ||
        text.find("\"driverMode\":\"usb\"") != std::string::npos ||
        text.find("\"driverMode\": \"pico\"") != std::string::npos ||
        text.find("\"driverMode\":\"pico\"") != std::string::npos) {
        return "usb";
    }
    return "unknown";
}

void WriteBool(std::ostringstream& out, const char* key, bool value) {
    out << key << "=" << (value ? "1" : "0") << "\n";
}

void WriteText(std::ostringstream& out, const char* key, const std::string& value) {
    out << key << "=" << value << "\n";
}

constexpr wchar_t kSingleInstanceMutexName[] = L"XiaoJiAutoKey_SingleInstance_Mutex";
constexpr wchar_t kActivateWindowEventName[] = L"XiaoJiAutoKey_Activate_Event";

// Ask the already-running instance to surface its main window. The instance
// that owns the single-instance mutex creates the activation event while it is
// still initializing, so a short retry covers the startup race. Returns false
// only when no running instance could be reached.
bool SignalRunningInstanceToShowWindow() {
    for (int attempt = 0; attempt < 30; ++attempt) {
        HANDLE activateEvent = OpenEventW(EVENT_MODIFY_STATE, FALSE, kActivateWindowEventName);
        if (activateEvent) {
            AllowSetForegroundWindow(ASFW_ANY);
            const BOOL signaled = SetEvent(activateEvent);
            CloseHandle(activateEvent);
            return signaled != FALSE;
        }
        Sleep(100);
    }
    return false;
}

}  // namespace

int main(int argc, char *argv[]) {
    const QStringList args(argv, argv + argc);
    if (args.contains(QStringLiteral("--install-agent"))) {
        // Agent installation is temporarily disabled.
        return 2;
    }
    if (args.contains(QStringLiteral("--probe-dd-hid"))) {
        const int index = args.indexOf(QStringLiteral("--probe-dd-hid"));
        if (index < 0 || index + 1 >= args.size()) {
            return 2;
        }
        DdDriver probeDriver;
        return probeDriver.LoadHidFrom(args.at(index + 1).toStdWString()) ? 0 : 3;
    }
    if (args.contains(QStringLiteral("--diagnose-driver"))) {
        AppPaths appPaths;
        const std::string rawConfigText = ReadFileText(appPaths.configPath());
        ConfigStore store(appPaths.configPath());
        const AppConfig config = store.Load();
        DdRuntimeService runtime(std::filesystem::path(appPaths.exeDir()), appPaths.logsDir());
        DdDriver driver;
        std::error_code error;
        std::filesystem::create_directories(appPaths.logsDir(), error);
        std::ostringstream reportText;

        const auto statusBefore = runtime.Inspect(driver);
        const bool uninstalledByUser = config.IsDriverUninstalledByUser(config.driverMode);
        bool loadAttempted = false;
        bool loadOk = false;
        bool loadedAsConfigured = false;
        std::string loadSkippedReason;
        if (uninstalledByUser) {
            loadSkippedReason = "The selected driver was explicitly uninstalled by the user.";
        } else if (config.driverMode == DriverMode::Usb) {
            loadSkippedReason = "USB connection is not probed by this command.";
        } else if (config.driverMode == DriverMode::Hid) {
            const bool installed = statusBefore.hidServiceInstalled && statusBefore.hidSysInstalled &&
                (statusBefore.hidRootDeviceOk || statusBefore.hidChildDeviceOk);
            if (statusBefore.hidRuntimeExists && installed && !statusBefore.hidServicePendingDelete) {
                loadAttempted = true;
                loadOk = driver.LoadHidFrom(statusBefore.hidRuntimePath);
                loadedAsConfigured = loadOk && driver.GetRuntimeKind() == DdDriver::RuntimeKind::Hid;
            } else {
                loadSkippedReason = "HID runtime or installed driver is not ready; diagnosis does not install drivers.";
            }
        } else {
            const bool installed = statusBefore.infKeyboardServiceInstalled && statusBefore.infMouseServiceInstalled &&
                statusBefore.infKeyboardSysInstalled && statusBefore.infMouseSysInstalled;
            if (statusBefore.legacyRuntimeExists && installed) {
                loadAttempted = true;
                loadOk = driver.LoadLegacyFromDirectory(appPaths.exeDir());
                loadedAsConfigured = loadOk && driver.GetRuntimeKind() == DdDriver::RuntimeKind::Legacy;
            } else {
                loadSkippedReason = "DD runtime or installed driver is not ready; diagnosis does not install drivers.";
            }
        }

        WriteText(reportText, "config_path", appPaths.configPath().string());
        WriteText(reportText, "config_driver_mode_raw_scan", DetectDriverModeFromRawConfig(rawConfigText));
        WriteText(reportText, "config_driver_mode", DriverModeText(config.driverMode));
        WriteBool(reportText, "status_hid_runtime_exists", statusBefore.hidRuntimeExists);
        WriteBool(reportText, "status_hid_service_installed", statusBefore.hidServiceInstalled);
        WriteBool(reportText, "status_hid_sys_installed", statusBefore.hidSysInstalled);
        WriteBool(reportText, "status_hid_root_device_ok", statusBefore.hidRootDeviceOk);
        WriteBool(reportText, "status_hid_child_device_ok", statusBefore.hidChildDeviceOk);
        WriteBool(reportText, "status_legacy32_exists", statusBefore.legacy32Exists);
        WriteBool(reportText, "status_legacy64_exists", statusBefore.legacy64Exists);
        WriteBool(reportText, "driver_uninstalled_by_user", uninstalledByUser);
        WriteBool(reportText, "load_attempted", loadAttempted);
        WriteText(reportText, "load_skipped_reason", loadSkippedReason);
        WriteBool(reportText, "probe_hid_ok", config.driverMode == DriverMode::Hid && loadOk);
        WriteText(reportText, "probe_hid_message", config.driverMode != DriverMode::Hid
            ? "Not run: HID is not the selected mode."
            : (!loadAttempted ? loadSkippedReason : (loadOk ? "HID runtime loaded." : ToUtf8Line(driver.GetLastError()))));
        WriteBool(reportText, "load_ok", loadOk);
        WriteBool(reportText, "loaded_as_configured", loadedAsConfigured);
        WriteText(reportText, "loaded_runtime_kind", RuntimeKindText(driver.GetRuntimeKind()));
        WriteBool(reportText, "driver_ready", driver.IsReady());
        WriteText(reportText, "driver_last_error", ToUtf8Line(driver.GetLastError()));

        const auto guards = CodeIntegrityInspector::Inspect(true);
        WriteBool(reportText, "guard_memory_integrity_running", guards.memoryIntegrityRunning);
        WriteBool(reportText, "guard_memory_integrity_configured", guards.memoryIntegrityConfigured);
        WriteBool(reportText, "guard_vulnerable_blocklist_enabled", guards.vulnerableBlocklistEnabled);
        WriteText(reportText, "guard_smart_app_control_state", std::to_string(guards.smartAppControlState));
        for (const auto& block : guards.recentBlocks) {
            WriteText(reportText, "guard_block_event",
                ToUtf8Line(block.timeText + L" " + block.fileName
                    + L" event=" + std::to_wstring(block.eventId)));
        }

        const std::string reportContent = reportText.str();
        std::ofstream report(appPaths.logsDir() / "driver-diagnose.log", std::ios::binary | std::ios::trunc);
        if (!report) {
            return 5;
        }
        report.write(reportContent.data(), static_cast<std::streamsize>(reportContent.size()));
        report.flush();
        return loadedAsConfigured ? 0 : 4;
    }

    if (args.contains(QStringLiteral("--restart-wait"))) {
        // Wait until the previous instance has fully released the
        // single-instance mutex before contending for it, so a restart or an
        // in-place update relaunch cannot lose the race and exit early.
        for (int attempt = 0; attempt < 100; ++attempt) {
            HANDLE existingMutex = OpenMutexW(SYNCHRONIZE, FALSE, kSingleInstanceMutexName);
            if (!existingMutex) {
                break;
            }
            WaitForSingleObject(existingMutex, 100);
            CloseHandle(existingMutex);
        }
    }

    if (args.contains(QStringLiteral("--agent"))) {
        // Do not keep legacy scheduled Agent processes alive.
        return 0;
    }

    HANDLE hMutex = CreateMutexW(nullptr, TRUE, kSingleInstanceMutexName);
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        if (hMutex) CloseHandle(hMutex);
        if (!SignalRunningInstanceToShowWindow()) {
            MessageBoxW(nullptr, L"\u5c0f\u53fd\u6309\u952e\u5df2\u5728\u8fd0\u884c\u4e2d\uff0c\u8bf7\u52ff\u91cd\u590d\u6253\u5f00\u3002", L"\u63d0\u793a", MB_OK | MB_ICONINFORMATION);
        }
        return 0;
    }

    // Only the owning instance may clean update files; a second launch must
    // leave the running instance's downloaded or staged package intact.
    UpdateService::cleanupPreviousUpdateArtifacts();

    HANDLE activateEvent = CreateEventW(nullptr, FALSE, FALSE, kActivateWindowEventName);

    PreserveTimerResolution();
    timeBeginPeriod(1);

    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
    QQuickStyle::setStyle(QStringLiteral("Basic"));
    QQuickWindow::setTextRenderType(QQuickWindow::NativeTextRendering);

    QApplication app(argc, argv);
    SpaceInputFilter spaceInputFilter;
    app.installEventFilter(&spaceInputFilter);
    app.setStyle(QStyleFactory::create("Fusion"));
    app.setPalette(Fluent::Theme::applicationPalette());

    QFont globalFont = Fluent::Typography::body();
    app.setFont(globalFont);

    MainWindow window;
    if (!window.Initialize()) {
        timeEndPeriod(1);
        if (activateEvent) CloseHandle(activateEvent);
        if (hMutex) CloseHandle(hMutex);
        return 0;
    }
    window.ShowForStartup();

    // A second launch signals the activation event instead of showing a
    // message box; poll it here and bring the window back to the foreground,
    // also when it is currently hidden in the tray.
    QTimer activationTimer(&window);
    activationTimer.setInterval(150);
    QObject::connect(&activationTimer, &QTimer::timeout, &window, [&window, activateEvent]() {
        if (!activateEvent || WaitForSingleObject(activateEvent, 0) != WAIT_OBJECT_0) {
            return;
        }
        window.showNormal();
        window.raise();
        window.activateWindow();
    });
    activationTimer.start();

    const int result = app.exec();

    activationTimer.stop();
    timeEndPeriod(1);
    if (activateEvent) CloseHandle(activateEvent);
    if (hMutex) CloseHandle(hMutex);
    return result;
}
