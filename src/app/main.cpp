#include "MainWindow.h"
#include "ConfigStore.h"
#include "DdDriver.h"
#include "services/AppPaths.h"
#include "services/DdRuntimeService.h"
#include "ui/styles/DesignTokens.h"

#include <QApplication>
#include <QKeyEvent>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QTextEdit>
#include <QCoreApplication>
#include <QFont>
#include <QGuiApplication>
#include <QStringList>
#include <QStyleFactory>
#include <QtPlugin>

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

// Static Qt plugin imports
Q_IMPORT_PLUGIN(QWindowsIntegrationPlugin)
Q_IMPORT_PLUGIN(QWindowsVistaStylePlugin)

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
                if (event->type() != QEvent::ShortcutOverride && isEditableTextTarget(watched)) {
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
        std::wstring probeMessage;
        const bool probeOk = runtime.ProbeHidRuntime(8000, &probeMessage);

        bool loadOk = false;
        bool loadedAsConfigured = false;
        if (config.driverMode == DriverMode::Usb) {
            loadOk = false;
            loadedAsConfigured = false;
        } else if (config.driverMode == DriverMode::Hid) {
            loadOk = driver.LoadFromDirectory(appPaths.exeDir(), true);
            loadedAsConfigured = loadOk && driver.GetRuntimeKind() == DdDriver::RuntimeKind::Hid;
        } else {
            loadOk = driver.LoadFromDirectory(appPaths.exeDir(), false);
            loadedAsConfigured = loadOk && driver.GetRuntimeKind() == DdDriver::RuntimeKind::Legacy;
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
        WriteBool(reportText, "probe_hid_ok", probeOk);
        WriteText(reportText, "probe_hid_message", ToUtf8Line(probeMessage));
        WriteBool(reportText, "load_ok", loadOk);
        WriteBool(reportText, "loaded_as_configured", loadedAsConfigured);
        WriteText(reportText, "loaded_runtime_kind", RuntimeKindText(driver.GetRuntimeKind()));
        WriteBool(reportText, "driver_ready", driver.IsReady());
        WriteText(reportText, "driver_last_error", ToUtf8Line(driver.GetLastError()));

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
        Sleep(1500);
    }

    if (args.contains(QStringLiteral("--agent"))) {
        // Do not keep legacy scheduled Agent processes alive.
        return 0;
    }

    HANDLE hMutex = CreateMutexW(nullptr, TRUE, L"XiaoJiAutoKey_SingleInstance_Mutex");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        if (hMutex) CloseHandle(hMutex);
        MessageBoxW(nullptr, L"\u5c0f\u53fd\u6309\u952e\u5df2\u5728\u8fd0\u884c\u4e2d\uff0c\u8bf7\u52ff\u91cd\u590d\u6253\u5f00\u3002", L"\u63d0\u793a", MB_OK | MB_ICONINFORMATION);
        return 0;
    }

    PreserveTimerResolution();
    timeBeginPeriod(1);

    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

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
        if (hMutex) CloseHandle(hMutex);
        return 0;
    }
    window.ShowForStartup();

    const int result = app.exec();

    timeEndPeriod(1);
    if (hMutex) CloseHandle(hMutex);
    return result;
}
