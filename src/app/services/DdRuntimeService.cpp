#include "services/DdRuntimeService.h"

#include <Windows.h>
#include <cfgmgr32.h>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>
#include <QString>

#include <chrono>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace {

constexpr const wchar_t* kHidServiceName = L"ddhid63340";
constexpr const wchar_t* kHidSysName = L"ddhid63340.sys";
constexpr const wchar_t* kHidRootDevicePrefix = L"ROOT\\HIDCLASS\\";
constexpr const wchar_t* kHidChildDevicePrefix = L"HID\\DDHID63340";
constexpr const wchar_t* kInfKeyboardServiceName = L"dd.key.96688";
constexpr const wchar_t* kInfMouseServiceName = L"dd.mou.96688";
constexpr const wchar_t* kInfKeyboardSysName = L"dd.key.96688.sys";
constexpr const wchar_t* kInfMouseSysName = L"dd.mou.96688.sys";

bool CopyResourceFile(const QString& resourcePath, const QString& outputPath, QString* errorMessage) {
    QFile source(resourcePath);
    if (!source.exists()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Embedded resource not found: %1").arg(resourcePath);
        }
        return false;
    }

    QFileInfo targetInfo(outputPath);
    QDir().mkpath(targetInfo.absolutePath());
    if (QFile::exists(outputPath) && !QFile::remove(outputPath)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Unable to replace file: %1").arg(outputPath);
        }
        return false;
    }
    if (!source.copy(outputPath)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Unable to extract resource to: %1").arg(outputPath);
        }
        return false;
    }
    return true;
}

std::wstring QuoteArg(const std::wstring& value) {
    std::wstring escaped;
    escaped.reserve(value.size() + 2);
    escaped.push_back(L'"');
    for (const wchar_t ch : value) {
        if (ch == L'"') {
            escaped.append(L"\\\"");
        } else {
            escaped.push_back(ch);
        }
    }
    escaped.push_back(L'"');
    return escaped;
}

std::wstring JoinArgs(const std::vector<std::wstring>& args) {
    std::wstring text;
    for (std::size_t i = 0; i < args.size(); ++i) {
        if (i > 0) {
            text.push_back(L' ');
        }
        text.append(QuoteArg(args[i]));
    }
    return text;
}

bool RunProcess(const std::wstring& program,
                const std::vector<std::wstring>& args,
                DWORD* exitCode,
                std::wstring* outputMessage,
                std::wstring* errorMessage,
                const std::wstring& workingDirectory = {},
                int timeoutMs = -1,
                const QByteArray& stdinData = {}) {
    QProcess process;
    process.setProgram(QString::fromStdWString(program));
    if (!workingDirectory.empty()) {
        process.setWorkingDirectory(QString::fromStdWString(workingDirectory));
    }
    QStringList qtArgs;
    for (const auto& arg : args) {
        qtArgs.push_back(QString::fromStdWString(arg));
    }
    process.setArguments(qtArgs);
    process.setProcessChannelMode(QProcess::SeparateChannels);
    process.start();
    if (!process.waitForStarted()) {
        if (errorMessage) {
            *errorMessage = L"Unable to start process: " + program;
        }
        return false;
    }

    if (!stdinData.isEmpty()) {
        process.write(stdinData);
        process.closeWriteChannel();
    }

    const bool finished = process.waitForFinished(timeoutMs);
    if (!finished) {
        process.kill();
        process.waitForFinished(3000);
        if (exitCode) {
            *exitCode = WAIT_TIMEOUT;
        }
        if (outputMessage) {
            const QString stdoutText = QString::fromLocal8Bit(process.readAllStandardOutput());
            const QString stderrText = QString::fromLocal8Bit(process.readAllStandardError());
            *outputMessage = (stdoutText + stderrText).toStdWString();
        }
        if (errorMessage) {
            *errorMessage = L"Process timed out after " + std::to_wstring(timeoutMs) + L" ms";
            if (outputMessage && !outputMessage->empty()) {
                *errorMessage += L": " + *outputMessage;
            }
        }
        return false;
    }

    const DWORD processExitCode = process.exitStatus() == QProcess::NormalExit
        ? static_cast<DWORD>(process.exitCode())
        : 1;
    if (exitCode) {
        *exitCode = processExitCode;
    }

    if (outputMessage) {
        const QString stdoutText = QString::fromLocal8Bit(process.readAllStandardOutput());
        const QString stderrText = QString::fromLocal8Bit(process.readAllStandardError());
        *outputMessage = (stdoutText + stderrText).toStdWString();
    }

    if (process.exitStatus() != QProcess::NormalExit || processExitCode != 0) {
        if (errorMessage) {
            std::wstring message = L"Process exited with code " + std::to_wstring(processExitCode);
            if (outputMessage && !outputMessage->empty()) {
                message += L": " + *outputMessage;
            }
            *errorMessage = std::move(message);
        }
        return false;
    }
    return true;
}

void AppendLog(const std::filesystem::path& logsDir, const wchar_t* fileName, const std::wstring& message) {
    if (logsDir.empty()) {
        return;
    }

    std::error_code error;
    std::filesystem::create_directories(logsDir, error);
    if (error) {
        return;
    }

    std::wofstream logFile(logsDir / fileName, std::ios::app);
    if (!logFile) {
        return;
    }

    const auto now = std::chrono::system_clock::now();
    const std::time_t nowTime = std::chrono::system_clock::to_time_t(now);
    std::tm localTime{};
    localtime_s(&localTime, &nowTime);
    logFile << L"[" << std::put_time(&localTime, L"%Y-%m-%d %H:%M:%S") << L"]\n"
            << message << L"\n";
}

}  // namespace

DdRuntimeService::DdRuntimeService(std::filesystem::path exeDir, std::filesystem::path logsDir)
    : exeDir_(std::move(exeDir)),
      logsDir_(std::move(logsDir)) {}

DdRuntimeService::RuntimeStatus DdRuntimeService::Inspect(const DdDriver& driver) const {
    RuntimeStatus status;
    status.driverReady = driver.IsReady();
    status.lastError = driver.GetLastError();
    status.hidRuntimeExists = std::filesystem::exists(HidRuntimePath());
    status.hidEmbeddedDriverExists = EmbeddedHidDriverExists();
    status.hidServiceInstalled = ServiceExists(kHidServiceName);
    status.hidSysInstalled = std::filesystem::exists(SystemDriversDir() / kHidSysName);
    status.hidRootDeviceOk = DeviceInstanceOk(kHidRootDevicePrefix);
    status.hidChildDeviceOk = DeviceInstanceOk(kHidChildDevicePrefix);
    status.legacy32Exists = std::filesystem::exists(Legacy32Path());
    status.legacy64Exists = std::filesystem::exists(Legacy64Path());
    status.embeddedInfVariant0Exists = EmbeddedInfVariantExists(0);
    status.embeddedInfVariant1Exists = EmbeddedInfVariantExists(1);
    status.infKeyboardServiceInstalled = ServiceExists(kInfKeyboardServiceName);
    status.infMouseServiceInstalled = ServiceExists(kInfMouseServiceName);
    status.infKeyboardSysInstalled = std::filesystem::exists(SystemDriversDir() / kInfKeyboardSysName);
    status.infMouseSysInstalled = std::filesystem::exists(SystemDriversDir() / kInfMouseSysName);
    status.candidateDlls = {
        HidRuntimePath().wstring(),
        Legacy64Path().wstring(),
        Legacy32Path().wstring(),
    };

    if (!status.driverReady) {
        status.notes.push_back(L"DD is not ready. Input simulation will fail until the INF driver and runtime DLL are both available.");
    }
    if (!status.hidRuntimeExists && !status.legacy32Exists && !status.legacy64Exists) {
        status.notes.push_back(L"No DD runtime files were found in the application directory.");
    }
    if (!status.hidEmbeddedDriverExists && !status.embeddedInfVariant0Exists && !status.embeddedInfVariant1Exists) {
        status.notes.push_back(L"No embedded INF driver package was found.");
    }

    return status;
}

bool DdRuntimeService::InstallEmbeddedHidDriver(std::wstring* resultMessage) const {
    const QString tempRoot = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    if (tempRoot.isEmpty()) {
        if (resultMessage) {
            *resultMessage = L"Unable to resolve the temp directory.";
        }
        return false;
    }

    wchar_t systemDir[MAX_PATH] = {};
    const UINT systemDirLen = GetSystemDirectoryW(systemDir, MAX_PATH);
    if (systemDirLen == 0 || systemDirLen >= MAX_PATH) {
        if (resultMessage) {
            *resultMessage = L"Unable to resolve the Windows system directory.";
        }
        return false;
    }
    const std::wstring pnputilPath = (std::filesystem::path(systemDir) / L"pnputil.exe").wstring();

    if (!EmbeddedHidDriverExists()) {
        if (resultMessage) {
            *resultMessage = L"No embedded HID driver package is available.";
        }
        return false;
    }

    const auto uniqueToken = std::to_wstring(GetCurrentProcessId()) + L"-" +
        std::to_wstring(static_cast<unsigned long long>(GetTickCount64()));
    const QString installRoot = QDir(tempRoot).filePath(QStringLiteral("xiaoji-dd-hid"));
    const QString outputDir = QDir(installRoot).filePath(QString::fromStdWString(uniqueToken));
    QDir().mkpath(outputDir);

    const QString ddcPath = QDir(outputDir).filePath(QStringLiteral("ddc.exe"));
    const QString infPath = QDir(outputDir).filePath(QStringLiteral("ddhid63340.inf"));
    const QString catPath = QDir(outputDir).filePath(QStringLiteral("ddhid63340.cat"));
    const QString sysPath = QDir(outputDir).filePath(QStringLiteral("ddhid63340.sys"));

    QString extractError;
    const QString prefix = QStringLiteral(":/dd/hid/drv/");
    std::wstringstream message;
    if (!CopyResourceFile(prefix + QStringLiteral("ddc.exe"), ddcPath, &extractError) ||
        !CopyResourceFile(prefix + QStringLiteral("ddhid63340.inf"), infPath, &extractError) ||
        !CopyResourceFile(prefix + QStringLiteral("ddhid63340.cat"), catPath, &extractError) ||
        !CopyResourceFile(prefix + QStringLiteral("ddhid63340.sys"), sysPath, &extractError)) {
        message << L"HID extract failed: " << extractError.toStdWString() << L"\n";
        if (resultMessage) {
            *resultMessage = message.str();
        }
        AppendLog(logsDir_, L"dd-hid-install.log", message.str());
        return false;
    }

    DWORD exitCode = 1;
    std::wstring stepOutput;
    std::wstring stepError;
    const bool addHidOk = RunProcess(pnputilPath,
        {L"/add-driver", infPath.toStdWString(), L"/install"},
        &exitCode,
        &stepOutput,
        &stepError);
    message << L"HID add inf -> "
            << (addHidOk ? L"ok" : (L"failed (" + stepError + L")")) << L"\n";

    const bool createDeviceOk = RunProcess(ddcPath.toStdWString(),
        {},
        &exitCode,
        &stepOutput,
        &stepError,
        outputDir.toStdWString(),
        5000);
    message << L"HID create device -> "
            << (createDeviceOk ? L"ok" : (L"failed (" + stepError + L")")) << L"\n";

    const bool serviceReady = ServiceExists(kHidServiceName);
    const bool sysReady = std::filesystem::exists(SystemDriversDir() / kHidSysName);
    const bool rootDeviceOk = DeviceInstanceOk(kHidRootDevicePrefix);
    const bool childDeviceOk = DeviceInstanceOk(kHidChildDevicePrefix);
    const bool overallInstalled = serviceReady && sysReady && rootDeviceOk && childDeviceOk;
    message << L"HID final state -> service="
            << (serviceReady ? L"yes" : L"no")
            << L", sys="
            << (sysReady ? L"yes" : L"no")
            << L", root_device="
            << (rootDeviceOk ? L"yes" : L"no")
            << L", child_device="
            << (childDeviceOk ? L"yes" : L"no")
            << L"\n";

    if (resultMessage) {
        std::wstringstream summary;
        summary << (overallInstalled ? L"HID install succeeded.\n" : L"HID install did not complete successfully.\n");
        summary << message.str();
        *resultMessage = summary.str();
    }

    std::wstringstream logMessage;
    logMessage << (overallInstalled ? L"HID install succeeded.\n" : L"HID install did not complete successfully.\n");
    logMessage << message.str();
    AppendLog(logsDir_, L"dd-hid-install.log", logMessage.str());
    return overallInstalled;
}

bool DdRuntimeService::InstallEmbeddedInfDriver(std::wstring* resultMessage) const {
    const QString tempRoot = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    if (tempRoot.isEmpty()) {
        if (resultMessage) {
            *resultMessage = L"Unable to resolve the temp directory.";
        }
        return false;
    }

    wchar_t systemDir[MAX_PATH] = {};
    const UINT systemDirLen = GetSystemDirectoryW(systemDir, MAX_PATH);
    if (systemDirLen == 0 || systemDirLen >= MAX_PATH) {
        if (resultMessage) {
            *resultMessage = L"Unable to resolve the Windows system directory.";
        }
        return false;
    }
    const std::wstring pnputilPath = (std::filesystem::path(systemDir) / L"pnputil.exe").wstring();

    std::vector<int> variants;
    if (EmbeddedInfVariantExists(1)) variants.push_back(1);
    if (EmbeddedInfVariantExists(0)) variants.push_back(0);
    if (variants.empty()) {
        if (resultMessage) {
            *resultMessage = L"No embedded INF driver package is available.";
        }
        return false;
    }

    std::wstringstream message;
    bool overallInstalled = false;
    int installedVariant = -1;
    const auto uniqueToken = std::to_wstring(GetCurrentProcessId()) + L"-" +
        std::to_wstring(static_cast<unsigned long long>(GetTickCount64()));

    for (const int variant : variants) {
        const QString installRoot = QDir(tempRoot).filePath(QStringLiteral("xiaoji-dd-inf"));
        const QString variantName = QString::number(variant);
        const QString outputDir = QDir(installRoot).filePath(
            QStringLiteral("%1-%2").arg(variantName, QString::fromStdWString(uniqueToken)));
        QDir().mkpath(outputDir);

        const QString dcPath = QDir(outputDir).filePath(QStringLiteral("dc64.exe"));
        const QString keyInfPath = QDir(outputDir).filePath(QStringLiteral("dd.key.96688.inf"));
        const QString keyCatPath = QDir(outputDir).filePath(QStringLiteral("dd.key.96688.cat"));
        const QString keySysPath = QDir(outputDir).filePath(QStringLiteral("dd.key.96688.sys"));
        const QString mouInfPath = QDir(outputDir).filePath(QStringLiteral("dd.mou.96688.inf"));
        const QString mouCatPath = QDir(outputDir).filePath(QStringLiteral("dd.mou.96688.cat"));
        const QString mouSysPath = QDir(outputDir).filePath(QStringLiteral("dd.mou.96688.sys"));

        QString extractError;
        const QString prefix = QStringLiteral(":/dd/inf/%1/").arg(variantName);
        if (!CopyResourceFile(prefix + QStringLiteral("dc64.exe"), dcPath, &extractError) ||
            !CopyResourceFile(prefix + QStringLiteral("dd.key.96688.inf"), keyInfPath, &extractError) ||
            !CopyResourceFile(prefix + QStringLiteral("dd.key.96688.cat"), keyCatPath, &extractError) ||
            !CopyResourceFile(prefix + QStringLiteral("dd.key.96688.sys"), keySysPath, &extractError) ||
            !CopyResourceFile(prefix + QStringLiteral("dd.mou.96688.inf"), mouInfPath, &extractError) ||
            !CopyResourceFile(prefix + QStringLiteral("dd.mou.96688.cat"), mouCatPath, &extractError) ||
            !CopyResourceFile(prefix + QStringLiteral("dd.mou.96688.sys"), mouSysPath, &extractError)) {
            message << L"variant " << variant << L": extract failed: " << extractError.toStdWString() << L"\n";
            continue;
        }

        DWORD exitCode = 1;
        std::wstring stepOutput;
        std::wstring stepError;
        const bool addKeyOk = RunProcess(pnputilPath,
            {L"/add-driver", keyInfPath.toStdWString(), L"/install"},
            &exitCode,
            &stepOutput,
            &stepError);
        message << L"variant " << variant << L": add key inf -> "
                << (addKeyOk ? L"ok" : (L"failed (" + stepError + L")")) << L"\n";

        const bool addMouOk = RunProcess(pnputilPath,
            {L"/add-driver", mouInfPath.toStdWString(), L"/install"},
            &exitCode,
            &stepOutput,
            &stepError);
        message << L"variant " << variant << L": add mouse inf -> "
                << (addMouOk ? L"ok" : (L"failed (" + stepError + L")")) << L"\n";

        const bool installKeyOk = RunProcess(dcPath.toStdWString(),
            {L"install", keyInfPath.toStdWString(), L"dd.key.96688"},
            &exitCode,
            &stepOutput,
            &stepError);
        message << L"variant " << variant << L": create key device -> "
                << (installKeyOk ? L"ok" : (L"failed (" + stepError + L")")) << L"\n";

        const bool installMouOk = RunProcess(dcPath.toStdWString(),
            {L"install", mouInfPath.toStdWString(), L"dd.mou.96688"},
            &exitCode,
            &stepOutput,
            &stepError);
        message << L"variant " << variant << L": create mouse device -> "
                << (installMouOk ? L"ok" : (L"failed (" + stepError + L")")) << L"\n";

        const bool servicesReady = ServiceExists(kInfKeyboardServiceName) && ServiceExists(kInfMouseServiceName);
        const bool sysReady = std::filesystem::exists(SystemDriversDir() / kInfKeyboardSysName) &&
                              std::filesystem::exists(SystemDriversDir() / kInfMouseSysName);
        overallInstalled = servicesReady && sysReady;
        message << L"variant " << variant << L": final state -> services="
                << (servicesReady ? L"yes" : L"no")
                << L", sys="
                << (sysReady ? L"yes" : L"no")
                << L"\n";
        if (overallInstalled) {
            installedVariant = variant;
            break;
        }
    }

    if (resultMessage) {
        std::wstringstream summary;
        if (overallInstalled) {
            summary << L"INF install succeeded. active_variant=" << installedVariant << L"\n";
        } else {
            summary << L"INF install did not complete successfully.\n";
        }
        summary << message.str();
        *resultMessage = summary.str();
    }
    std::wstringstream logMessage;
    if (overallInstalled) {
        logMessage << L"INF install succeeded. active_variant=" << installedVariant << L"\n";
    } else {
        logMessage << L"INF install did not complete successfully.\n";
    }
    logMessage << message.str();
    AppendLog(logsDir_, L"dd-inf-install.log", logMessage.str());
    return overallInstalled;
}

bool DdRuntimeService::UninstallEmbeddedHidDriver(std::wstring* resultMessage) const {
    wchar_t systemDir[MAX_PATH] = {};
    const UINT systemDirLen = GetSystemDirectoryW(systemDir, MAX_PATH);
    if (systemDirLen == 0 || systemDirLen >= MAX_PATH) {
        if (resultMessage) {
            *resultMessage = L"Unable to resolve the Windows system directory.";
        }
        return false;
    }
    const std::filesystem::path systemPath(systemDir);
    const std::wstring pnputilPath = (systemPath / L"pnputil.exe").wstring();
    const std::wstring scPath = (systemPath / L"sc.exe").wstring();

    std::wstringstream message;
    bool removedDevice = false;
    const std::filesystem::path packagedDdc = HidRootPath() / L"drv" / L"ddc.exe";
    const bool hasHidDevices = DeviceInstanceOk(kHidRootDevicePrefix) || DeviceInstanceOk(kHidChildDevicePrefix);
    if (hasHidDevices && std::filesystem::exists(packagedDdc)) {
        DWORD exitCode = 1;
        std::wstring stepOutput;
        std::wstring stepError;
        removedDevice = RunProcess(
            packagedDdc.wstring(),
            {},
            &exitCode,
            &stepOutput,
            &stepError,
            packagedDdc.parent_path().wstring(),
            15000,
            QByteArray("Y\r\nY\r\n"));
        message << L"HID remove device -> "
                << (removedDevice ? L"ok" : (L"failed (" + stepError + L")")) << L"\n";
        if (!stepOutput.empty()) {
            message << stepOutput << L"\n";
        }
    } else {
        message << L"HID remove device -> skipped ("
                << (hasHidDevices ? L"ddc.exe not found" : L"device not present")
                << L")\n";
    }

    DWORD enumExitCode = 1;
    std::wstring enumOutput;
    std::wstring enumError;
    const bool enumOk = RunProcess(
        pnputilPath,
        {L"/enum-drivers"},
        &enumExitCode,
        &enumOutput,
        &enumError,
        {},
        15000);

    std::vector<std::wstring> publishedNames;
    if (enumOk) {
        std::wistringstream input(enumOutput);
        std::wstring line;
        std::wstring currentPublished;
        while (std::getline(input, line)) {
            if (!line.empty() && line.back() == L'\r') {
                line.pop_back();
            }
            const auto colon = line.find(L':');
            if (colon == std::wstring::npos) {
                continue;
            }
            std::wstring key = line.substr(0, colon);
            std::wstring value = line.substr(colon + 1);
            auto trim = [](std::wstring text) {
                const auto first = text.find_first_not_of(L" \t");
                if (first == std::wstring::npos) return std::wstring();
                const auto last = text.find_last_not_of(L" \t");
                return text.substr(first, last - first + 1);
            };
            key = trim(key);
            value = trim(value);
            if (_wcsicmp(key.c_str(), L"Published Name") == 0) {
                currentPublished = value;
            } else if (_wcsicmp(key.c_str(), L"Original Name") == 0 &&
                       _wcsicmp(value.c_str(), L"ddhid63340.inf") == 0 &&
                       !currentPublished.empty()) {
                publishedNames.push_back(currentPublished);
            }
        }
    } else {
        message << L"HID enum driver packages -> failed (" << enumError << L")\n";
    }

    bool removedPackage = publishedNames.empty();
    for (const auto& publishedName : publishedNames) {
        DWORD exitCode = 1;
        std::wstring stepOutput;
        std::wstring stepError;
        const bool deleted = RunProcess(
            pnputilPath,
            {L"/delete-driver", publishedName, L"/uninstall", L"/force"},
            &exitCode,
            &stepOutput,
            &stepError,
            {},
            30000);
        message << L"HID delete driver " << publishedName << L" -> "
                << (deleted ? L"ok" : (L"failed (" + stepError + L")")) << L"\n";
        removedPackage = removedPackage && deleted;
    }

    DWORD scExitCode = 1;
    std::wstring scOutput;
    std::wstring scError;
    const bool serviceDeleted = RunProcess(
        scPath,
        {L"delete", kHidServiceName},
        &scExitCode,
        &scOutput,
        &scError,
        {},
        10000);
    message << L"HID delete service -> "
            << (serviceDeleted ? L"ok" : (L"failed (" + scError + L")")) << L"\n";

    std::error_code removeError;
    const auto sysPath = SystemDriversDir() / kHidSysName;
    const bool sysRemoved = !std::filesystem::exists(sysPath) || std::filesystem::remove(sysPath, removeError);
    message << L"HID remove sys -> "
            << (sysRemoved ? L"ok" : L"failed")
            << L"\n";

    const bool stillHasRoot = DeviceInstanceOk(kHidRootDevicePrefix);
    const bool stillHasChild = DeviceInstanceOk(kHidChildDevicePrefix);
    const bool stillHasService = ServiceExists(kHidServiceName);
    const bool overallUninstalled = !stillHasRoot && !stillHasChild && !stillHasService;
    message << L"HID final state -> root_device="
            << (stillHasRoot ? L"yes" : L"no")
            << L", child_device="
            << (stillHasChild ? L"yes" : L"no")
            << L", service="
            << (stillHasService ? L"yes" : L"no")
            << L"\n";

    if (resultMessage) {
        std::wstringstream summary;
        summary << (overallUninstalled ? L"HID uninstall succeeded.\n" : L"HID uninstall did not fully complete.\n");
        summary << message.str();
        *resultMessage = summary.str();
    }
    std::wstringstream logMessage;
    logMessage << (overallUninstalled ? L"HID uninstall succeeded.\n" : L"HID uninstall did not fully complete.\n");
    logMessage << message.str();
    AppendLog(logsDir_, L"dd-hid-uninstall.log", logMessage.str());
    return overallUninstalled;
}

bool DdRuntimeService::ProbeHidRuntime(int timeoutMs, std::wstring* resultMessage) const {
    wchar_t exePath[MAX_PATH] = {};
    const DWORD exePathLen = GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    if (exePathLen == 0 || exePathLen >= MAX_PATH) {
        if (resultMessage) {
            *resultMessage = L"Unable to resolve the executable path.";
        }
        return false;
    }

    DWORD exitCode = 1;
    std::wstring output;
    std::wstring error;
    const bool ok = RunProcess(
        exePath,
        {L"--probe-dd-hid", HidRuntimePath().wstring()},
        &exitCode,
        &output,
        &error,
        ExeDir().wstring(),
        timeoutMs);

    if (resultMessage) {
        std::wstringstream message;
        message << L"HID runtime probe -> ";
        if (ok && exitCode == 0) {
            message << L"ok";
        } else if (!error.empty()) {
            message << L"failed (" << error << L")";
        } else {
            message << L"failed (exit_code=" << exitCode << L")";
        }
        if (!output.empty()) {
            message << L"\n" << output;
        }
        *resultMessage = message.str();
    }
    AppendLog(logsDir_, L"dd-hid-probe.log", resultMessage ? *resultMessage : (ok ? L"HID runtime probe -> ok" : L"HID runtime probe -> failed"));
    return ok && exitCode == 0;
}

std::filesystem::path DdRuntimeService::ExeDir() const noexcept {
    return exeDir_;
}

std::filesystem::path DdRuntimeService::LogsDir() const noexcept {
    return logsDir_;
}

std::filesystem::path DdRuntimeService::DriverRootDir() const {
    return exeDir_ / L"driver";
}

std::filesystem::path DdRuntimeService::HidRootPath() const {
    return DriverRootDir() / L"hid";
}

std::filesystem::path DdRuntimeService::HidRuntimePath() const {
    return HidRootPath() / L"ddhid.63340.dll";
}

bool DdRuntimeService::EmbeddedInfVariantExists(int variant) const {
    const QString variantPath = QStringLiteral(":/dd/inf/%1/dd.key.96688.inf").arg(variant);
    return QFile::exists(variantPath);
}

bool DdRuntimeService::EmbeddedHidDriverExists() const {
    return QFile::exists(QStringLiteral(":/dd/hid/drv/ddhid63340.inf"));
}

bool DdRuntimeService::DeviceInstanceOk(const wchar_t* instanceIdPrefix) const {
    if (instanceIdPrefix == nullptr || *instanceIdPrefix == L'\0') {
        return false;
    }

    ULONG deviceIdListSize = 0;
    CONFIGRET result = CM_Get_Device_ID_List_SizeW(&deviceIdListSize, nullptr, CM_GETIDLIST_FILTER_NONE);
    if (result != CR_SUCCESS || deviceIdListSize == 0) {
        return false;
    }

    std::vector<wchar_t> deviceIds(deviceIdListSize);
    result = CM_Get_Device_ID_ListW(nullptr, deviceIds.data(), deviceIdListSize, CM_GETIDLIST_FILTER_NONE);
    if (result != CR_SUCCESS) {
        return false;
    }

    const std::wstring prefix(instanceIdPrefix);
    for (const wchar_t* id = deviceIds.data(); id != nullptr && *id != L'\0'; id += wcslen(id) + 1) {
        const std::wstring instanceId(id);
        if (instanceId.size() < prefix.size() ||
            _wcsnicmp(instanceId.c_str(), prefix.c_str(), prefix.size()) != 0) {
            continue;
        }

        DEVINST devInst = 0;
        if (CM_Locate_DevNodeW(&devInst, const_cast<DEVINSTID_W>(id), CM_LOCATE_DEVNODE_NORMAL) != CR_SUCCESS) {
            continue;
        }

        ULONG status = 0;
        ULONG problem = 0;
        if (CM_Get_DevNode_Status(&status, &problem, devInst, 0) == CR_SUCCESS && problem == 0) {
            return true;
        }
    }
    return false;
}

bool DdRuntimeService::ServiceExists(const wchar_t* serviceName) const {
    if (serviceName == nullptr || *serviceName == L'\0') {
        return false;
    }
    SC_HANDLE scm = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (scm == nullptr) {
        return false;
    }
    SC_HANDLE service = OpenServiceW(scm, serviceName, SERVICE_QUERY_STATUS);
    if (service != nullptr) {
        CloseServiceHandle(service);
        CloseServiceHandle(scm);
        return true;
    }
    CloseServiceHandle(scm);
    return false;
}

std::filesystem::path DdRuntimeService::SystemDriversDir() const {
    wchar_t windowsDir[MAX_PATH] = {};
    const UINT length = GetWindowsDirectoryW(windowsDir, MAX_PATH);
    if (length == 0 || length >= MAX_PATH) {
        return std::filesystem::path(L"C:\\Windows\\System32\\drivers");
    }
    return std::filesystem::path(windowsDir) / L"System32" / L"drivers";
}

std::filesystem::path DdRuntimeService::Legacy32Path() const {
    return DriverRootDir() / L"DD32.dll";
}

std::filesystem::path DdRuntimeService::Legacy64Path() const {
    return DriverRootDir() / L"DD64.dll";
}
