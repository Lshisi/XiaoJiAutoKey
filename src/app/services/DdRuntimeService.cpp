#include "services/DdRuntimeService.h"

#include <Windows.h>
#include <cfgmgr32.h>
#include <SetupAPI.h>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>
#include <QString>

#include <chrono>
#include <algorithm>
#include <cwctype>
#include <functional>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace {

constexpr const wchar_t* kInfKeyboardServiceName = L"dd.key.96688";
constexpr const wchar_t* kInfMouseServiceName = L"dd.mou.96688";
constexpr const wchar_t* kInfKeyboardSysName = L"dd.key.96688.sys";
constexpr const wchar_t* kInfMouseSysName = L"dd.mou.96688.sys";
constexpr const wchar_t* kInfInstallLogName = L"dd-inf-install.log";
constexpr const wchar_t* kInfKeyboardInstanceId = L"ROOT\\KEYBOARD\\0000";
constexpr const wchar_t* kInfMouseInstanceId = L"ROOT\\MOUSE\\0000";

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
                std::wstring* errorMessage) {
    QProcess process;
    process.setProgram(QString::fromStdWString(program));
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

    process.waitForFinished(-1);
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

std::wstring ToLower(std::wstring value) {
    std::transform(value.begin(), value.end(), value.begin(), [](wchar_t ch) {
        return static_cast<wchar_t>(std::towlower(ch));
    });
    return value;
}

bool ContainsText(const std::wstring& text, const std::wstring& needle) {
    return text.find(needle) != std::wstring::npos;
}

bool PnputilAddDriverSucceeded(bool processOk, DWORD exitCode, const std::wstring& output) {
    if (processOk || exitCode == ERROR_SUCCESS_REBOOT_REQUIRED) {
        return true;
    }

    const std::wstring lowerOutput = ToLower(output);
    if (ContainsText(lowerOutput, L"failed") ||
        ContainsText(lowerOutput, L"失败") ||
        ContainsText(lowerOutput, L"未能")) {
        return false;
    }

    return ContainsText(lowerOutput, L"driver package added successfully") ||
           ContainsText(lowerOutput, L"驱动程序包已成功添加") ||
           ContainsText(lowerOutput, L"已成功添加驱动程序包");
}

bool DevconDeviceCommandSucceeded(bool processOk, DWORD exitCode) {
    return processOk || exitCode == ERROR_SUCCESS_REBOOT_REQUIRED;
}

struct InfDeviceState {
    bool present = false;
    bool started = false;
    bool boundToService = false;
};

bool DeviceBoundToService(const wchar_t* instanceId, const wchar_t* serviceName) {
    HDEVINFO deviceInfo = SetupDiCreateDeviceInfoList(nullptr, nullptr);
    if (deviceInfo == INVALID_HANDLE_VALUE) {
        return false;
    }

    SP_DEVINFO_DATA deviceData{};
    deviceData.cbSize = sizeof(deviceData);
    if (!SetupDiOpenDeviceInfoW(deviceInfo, instanceId, nullptr, 0, &deviceData)) {
        SetupDiDestroyDeviceInfoList(deviceInfo);
        return false;
    }

    wchar_t serviceBuffer[256] = {};
    DWORD propertyType = 0;
    DWORD requiredSize = 0;
    const BOOL propertyOk = SetupDiGetDeviceRegistryPropertyW(
        deviceInfo,
        &deviceData,
        SPDRP_SERVICE,
        &propertyType,
        reinterpret_cast<PBYTE>(serviceBuffer),
        sizeof(serviceBuffer),
        &requiredSize);
    SetupDiDestroyDeviceInfoList(deviceInfo);

    return propertyOk &&
           propertyType == REG_SZ &&
           _wcsicmp(serviceBuffer, serviceName) == 0;
}

InfDeviceState QueryInfDeviceState(const wchar_t* instanceId, const wchar_t* serviceName) {
    InfDeviceState state;
    DEVINST devInst = 0;
    const CONFIGRET locateResult = CM_Locate_DevNodeW(&devInst, const_cast<DEVINSTID_W>(instanceId), CM_LOCATE_DEVNODE_NORMAL);
    if (locateResult != CR_SUCCESS) {
        return state;
    }

    state.present = true;

    ULONG status = 0;
    ULONG problem = 0;
    const CONFIGRET statusResult = CM_Get_DevNode_Status(&status, &problem, devInst, 0);
    state.started = statusResult == CR_SUCCESS &&
                    (status & DN_STARTED) != 0 &&
                    (status & DN_HAS_PROBLEM) == 0;
    state.boundToService = DeviceBoundToService(instanceId, serviceName);
    return state;
}

std::wstring FormatStepResult(bool accepted,
                              bool processOk,
                              DWORD exitCode,
                              const std::wstring& output,
                              const std::wstring& error) {
    std::wstringstream text;
    if (accepted) {
        text << L"ok";
        if (!processOk) {
            text << L" (exit=" << exitCode << L")";
        }
    } else {
        text << L"failed";
        if (exitCode != 0) {
            text << L" (exit=" << exitCode << L")";
        }
        if (!error.empty()) {
            text << L" (" << error << L")";
        }
    }
    if (!output.empty()) {
        text << L"\n" << output;
    }
    return text.str();
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

struct InfVariantPaths {
    QString outputDir;
    QString dcPath;
    QString keyInfPath;
    QString keyCatPath;
    QString keySysPath;
    QString mouInfPath;
    QString mouCatPath;
    QString mouSysPath;
};

bool ExtractInfVariant(int variant, const QString& uniqueToken, InfVariantPaths* paths, std::wstring* message) {
    if (!paths) {
        return false;
    }

    const QString tempRoot = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    if (tempRoot.isEmpty()) {
        if (message) {
            *message += L"Unable to resolve the temp directory.\n";
        }
        return false;
    }

    const QString installRoot = QDir(tempRoot).filePath(QStringLiteral("xiaoji-dd-inf"));
    const QString variantName = QString::number(variant);
    paths->outputDir = QDir(installRoot).filePath(QStringLiteral("%1-%2").arg(variantName, uniqueToken));
    QDir().mkpath(paths->outputDir);

    paths->dcPath = QDir(paths->outputDir).filePath(QStringLiteral("dc64.exe"));
    paths->keyInfPath = QDir(paths->outputDir).filePath(QStringLiteral("dd.key.96688.inf"));
    paths->keyCatPath = QDir(paths->outputDir).filePath(QStringLiteral("dd.key.96688.cat"));
    paths->keySysPath = QDir(paths->outputDir).filePath(QStringLiteral("dd.key.96688.sys"));
    paths->mouInfPath = QDir(paths->outputDir).filePath(QStringLiteral("dd.mou.96688.inf"));
    paths->mouCatPath = QDir(paths->outputDir).filePath(QStringLiteral("dd.mou.96688.cat"));
    paths->mouSysPath = QDir(paths->outputDir).filePath(QStringLiteral("dd.mou.96688.sys"));

    QString extractError;
    const QString prefix = QStringLiteral(":/dd/inf/%1/").arg(variantName);
    if (!CopyResourceFile(prefix + QStringLiteral("dc64.exe"), paths->dcPath, &extractError) ||
        !CopyResourceFile(prefix + QStringLiteral("dd.key.96688.inf"), paths->keyInfPath, &extractError) ||
        !CopyResourceFile(prefix + QStringLiteral("dd.key.96688.cat"), paths->keyCatPath, &extractError) ||
        !CopyResourceFile(prefix + QStringLiteral("dd.key.96688.sys"), paths->keySysPath, &extractError) ||
        !CopyResourceFile(prefix + QStringLiteral("dd.mou.96688.inf"), paths->mouInfPath, &extractError) ||
        !CopyResourceFile(prefix + QStringLiteral("dd.mou.96688.cat"), paths->mouCatPath, &extractError) ||
        !CopyResourceFile(prefix + QStringLiteral("dd.mou.96688.sys"), paths->mouSysPath, &extractError)) {
        if (message) {
            *message += L"extract failed: " + extractError.toStdWString() + L"\n";
        }
        return false;
    }

    return true;
}

std::wstring ResolvePnputilPath() {
    wchar_t systemDir[MAX_PATH] = {};
    const UINT systemDirLen = GetSystemDirectoryW(systemDir, MAX_PATH);
    if (systemDirLen == 0 || systemDirLen >= MAX_PATH) {
        return {};
    }
    return (std::filesystem::path(systemDir) / L"pnputil.exe").wstring();
}

bool InstallInfVariant(int variant,
                       const std::wstring& pnputilPath,
                       const QString& uniqueToken,
                       bool removeExistingDevices,
                       std::wstringstream* message) {
    InfVariantPaths paths;
    std::wstring extractMessage;
    if (!ExtractInfVariant(variant, uniqueToken, &paths, &extractMessage)) {
        if (message) {
            *message << L"variant " << variant << L": " << extractMessage;
        }
        return false;
    }

    DWORD exitCode = 1;
    std::wstring stepOutput;
    std::wstring stepError;

    if (removeExistingDevices) {
        stepOutput.clear();
        stepError.clear();
        exitCode = 1;
        const bool removeKeyOk = RunProcess(paths.dcPath.toStdWString(),
            {L"remove", L"dd.key.96688"},
            &exitCode,
            &stepOutput,
            &stepError);
        const bool removeKeyAccepted = DevconDeviceCommandSucceeded(removeKeyOk, exitCode);
        if (message) {
            *message << L"variant " << variant << L": remove key device -> "
                     << FormatStepResult(removeKeyAccepted, removeKeyOk, exitCode, stepOutput, stepError) << L"\n";
        }

        stepOutput.clear();
        stepError.clear();
        exitCode = 1;
        const bool removeMouOk = RunProcess(paths.dcPath.toStdWString(),
            {L"remove", L"dd.mou.96688"},
            &exitCode,
            &stepOutput,
            &stepError);
        const bool removeMouAccepted = DevconDeviceCommandSucceeded(removeMouOk, exitCode);
        if (message) {
            *message << L"variant " << variant << L": remove mouse device -> "
                     << FormatStepResult(removeMouAccepted, removeMouOk, exitCode, stepOutput, stepError) << L"\n";
        }
    }

    stepOutput.clear();
    stepError.clear();
    exitCode = 1;
    const bool addKeyProcessOk = RunProcess(pnputilPath,
        {L"/add-driver", paths.keyInfPath.toStdWString(), L"/install"},
        &exitCode,
        &stepOutput,
        &stepError);
    const bool addKeyOk = PnputilAddDriverSucceeded(addKeyProcessOk, exitCode, stepOutput);
    if (message) {
        *message << L"variant " << variant << L": add key inf -> "
                 << FormatStepResult(addKeyOk, addKeyProcessOk, exitCode, stepOutput, stepError) << L"\n";
    }

    stepOutput.clear();
    stepError.clear();
    exitCode = 1;
    const bool addMouProcessOk = RunProcess(pnputilPath,
        {L"/add-driver", paths.mouInfPath.toStdWString(), L"/install"},
        &exitCode,
        &stepOutput,
        &stepError);
    const bool addMouOk = PnputilAddDriverSucceeded(addMouProcessOk, exitCode, stepOutput);
    if (message) {
        *message << L"variant " << variant << L": add mouse inf -> "
                 << FormatStepResult(addMouOk, addMouProcessOk, exitCode, stepOutput, stepError) << L"\n";
    }

    stepOutput.clear();
    stepError.clear();
    exitCode = 1;
    const bool updateKeyProcessOk = RunProcess(paths.dcPath.toStdWString(),
        {L"update", paths.keyInfPath.toStdWString(), L"dd.key.96688"},
        &exitCode,
        &stepOutput,
        &stepError);
    const bool updateKeyOk = DevconDeviceCommandSucceeded(updateKeyProcessOk, exitCode);
    if (message) {
        *message << L"variant " << variant << L": update key device -> "
                 << FormatStepResult(updateKeyOk, updateKeyProcessOk, exitCode, stepOutput, stepError) << L"\n";
    }

    stepOutput.clear();
    stepError.clear();
    exitCode = 1;
    const bool updateMouProcessOk = RunProcess(paths.dcPath.toStdWString(),
        {L"update", paths.mouInfPath.toStdWString(), L"dd.mou.96688"},
        &exitCode,
        &stepOutput,
        &stepError);
    const bool updateMouOk = DevconDeviceCommandSucceeded(updateMouProcessOk, exitCode);
    if (message) {
        *message << L"variant " << variant << L": update mouse device -> "
                 << FormatStepResult(updateMouOk, updateMouProcessOk, exitCode, stepOutput, stepError) << L"\n";
    }

    stepOutput.clear();
    stepError.clear();
    exitCode = 1;
    const bool installKeyOk = RunProcess(paths.dcPath.toStdWString(),
        {L"install", paths.keyInfPath.toStdWString(), L"dd.key.96688"},
        &exitCode,
        &stepOutput,
        &stepError);
    const bool installKeyAccepted = DevconDeviceCommandSucceeded(installKeyOk, exitCode);
    if (message) {
        *message << L"variant " << variant << L": create key device -> "
                 << FormatStepResult(installKeyAccepted, installKeyOk, exitCode, stepOutput, stepError) << L"\n";
    }

    stepOutput.clear();
    stepError.clear();
    exitCode = 1;
    const bool installMouOk = RunProcess(paths.dcPath.toStdWString(),
        {L"install", paths.mouInfPath.toStdWString(), L"dd.mou.96688"},
        &exitCode,
        &stepOutput,
        &stepError);
    const bool installMouAccepted = DevconDeviceCommandSucceeded(installMouOk, exitCode);
    if (message) {
        *message << L"variant " << variant << L": create mouse device -> "
                 << FormatStepResult(installMouAccepted, installMouOk, exitCode, stepOutput, stepError) << L"\n";
    }

    stepOutput.clear();
    stepError.clear();
    exitCode = 1;
    const bool enableKeyProcessOk = RunProcess(paths.dcPath.toStdWString(),
        {L"enable", L"dd.key.96688"},
        &exitCode,
        &stepOutput,
        &stepError);
    const bool enableKeyOk = DevconDeviceCommandSucceeded(enableKeyProcessOk, exitCode);
    if (message) {
        *message << L"variant " << variant << L": enable key device -> "
                 << FormatStepResult(enableKeyOk, enableKeyProcessOk, exitCode, stepOutput, stepError) << L"\n";
    }

    stepOutput.clear();
    stepError.clear();
    exitCode = 1;
    const bool enableMouProcessOk = RunProcess(paths.dcPath.toStdWString(),
        {L"enable", L"dd.mou.96688"},
        &exitCode,
        &stepOutput,
        &stepError);
    const bool enableMouOk = DevconDeviceCommandSucceeded(enableMouProcessOk, exitCode);
    if (message) {
        *message << L"variant " << variant << L": enable mouse device -> "
                 << FormatStepResult(enableMouOk, enableMouProcessOk, exitCode, stepOutput, stepError) << L"\n";
    }

    return addKeyOk && addMouOk &&
           ((updateKeyOk && updateMouOk) || (installKeyAccepted && installMouAccepted) || (enableKeyOk && enableMouOk));
}

}  // namespace

DdRuntimeService::DdRuntimeService(std::filesystem::path exeDir, std::filesystem::path logsDir)
    : exeDir_(std::move(exeDir)),
      logsDir_(std::move(logsDir)) {}

DdRuntimeService::RuntimeStatus DdRuntimeService::Inspect(const DdDriver& driver) const {
    RuntimeStatus status;
    status.driverReady = driver.IsReady();
    status.lastError = driver.GetLastError();
    status.legacy32Exists = std::filesystem::exists(Legacy32Path());
    status.legacy64Exists = std::filesystem::exists(Legacy64Path());
    status.embeddedInfVariant0Exists = EmbeddedInfVariantExists(0);
    status.embeddedInfVariant1Exists = EmbeddedInfVariantExists(1);
    status.infKeyboardServiceInstalled = ServiceExists(kInfKeyboardServiceName);
    status.infMouseServiceInstalled = ServiceExists(kInfMouseServiceName);
    status.infKeyboardSysInstalled = std::filesystem::exists(SystemDriversDir() / kInfKeyboardSysName);
    status.infMouseSysInstalled = std::filesystem::exists(SystemDriversDir() / kInfMouseSysName);
    const InfDeviceState keyDevice = QueryInfDeviceState(kInfKeyboardInstanceId, kInfKeyboardServiceName);
    const InfDeviceState mouseDevice = QueryInfDeviceState(kInfMouseInstanceId, kInfMouseServiceName);
    status.infKeyboardDeviceStarted = keyDevice.started;
    status.infMouseDeviceStarted = mouseDevice.started;
    status.infKeyboardDeviceBound = keyDevice.boundToService;
    status.infMouseDeviceBound = mouseDevice.boundToService;
    status.candidateDlls = {
        Legacy64Path().wstring(),
        Legacy32Path().wstring(),
    };

    if (!status.driverReady) {
        status.notes.push_back(L"DD is not ready. Input simulation will fail until the INF driver and runtime DLL are both available.");
    }
    if (!status.legacy32Exists && !status.legacy64Exists) {
        status.notes.push_back(L"No DD runtime files were found in the application directory.");
    }
    if (!status.embeddedInfVariant0Exists && !status.embeddedInfVariant1Exists) {
        status.notes.push_back(L"No embedded INF driver package was found.");
    }
    if (!status.infKeyboardDeviceStarted || !status.infMouseDeviceStarted ||
        !status.infKeyboardDeviceBound || !status.infMouseDeviceBound) {
        status.notes.push_back(L"DD INF device instances are not fully started and bound to their services.");
    }

    return status;
}

bool DdRuntimeService::InstallEmbeddedInfDriver(std::wstring* resultMessage) const {
    const std::wstring pnputilPath = ResolvePnputilPath();
    if (pnputilPath.empty()) {
        if (resultMessage) {
            *resultMessage = L"Unable to resolve the Windows system directory.";
        }
        return false;
    }

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
    const QString uniqueToken = QString::fromStdWString(std::to_wstring(GetCurrentProcessId()) + L"-" +
        std::to_wstring(static_cast<unsigned long long>(GetTickCount64())));

    for (const int variant : variants) {
        InstallInfVariant(variant, pnputilPath, uniqueToken, false, &message);

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
    AppendLog(logsDir_, kInfInstallLogName, logMessage.str());
    return overallInstalled;
}

bool DdRuntimeService::RepairEmbeddedInfDriver(const std::function<bool()>& readinessProbe,
                                               std::wstring* resultMessage) const {
    const std::wstring pnputilPath = ResolvePnputilPath();
    if (pnputilPath.empty()) {
        if (resultMessage) {
            *resultMessage = L"Unable to resolve the Windows system directory.";
        }
        return false;
    }

    std::vector<int> variants;
    if (EmbeddedInfVariantExists(1)) variants.push_back(1);
    if (EmbeddedInfVariantExists(0)) variants.push_back(0);
    if (variants.empty()) {
        if (resultMessage) {
            *resultMessage = L"No embedded INF driver package is available.";
        }
        return false;
    }

    if (readinessProbe && readinessProbe()) {
        if (resultMessage) {
            *resultMessage = L"DD runtime is already ready; repair skipped.";
        }
        return true;
    }

    std::wstringstream message;
    bool repaired = false;
    int repairedVariant = -1;
    const QString uniqueToken = QString::fromStdWString(std::to_wstring(GetCurrentProcessId()) + L"-repair-" +
        std::to_wstring(static_cast<unsigned long long>(GetTickCount64())));

    for (const int variant : variants) {
        message << L"variant " << variant << L": repair begin (refresh existing devices)\n";
        InstallInfVariant(variant, pnputilPath, uniqueToken, false, &message);
        if (readinessProbe && readinessProbe()) {
            repaired = true;
            repairedVariant = variant;
            message << L"variant " << variant << L": readiness probe -> ok\n";
            break;
        }
        message << L"variant " << variant << L": readiness probe after refresh -> failed\n";

        message << L"variant " << variant << L": repair retry (remove stale devices)\n";
        InstallInfVariant(variant, pnputilPath, uniqueToken, true, &message);
        if (readinessProbe && readinessProbe()) {
            repaired = true;
            repairedVariant = variant;
            message << L"variant " << variant << L": readiness probe -> ok\n";
            break;
        }
        message << L"variant " << variant << L": readiness probe after rebuild -> failed\n";
    }

    std::wstringstream summary;
    if (repaired) {
        summary << L"INF repair succeeded. active_variant=" << repairedVariant << L"\n";
    } else {
        summary << L"INF repair did not restore DD runtime readiness.\n";
    }
    summary << message.str();

    if (resultMessage) {
        *resultMessage = summary.str();
    }
    AppendLog(logsDir_, kInfInstallLogName, summary.str());
    return repaired;
}

void DdRuntimeService::LogRuntimeReady(const std::wstring& source) const {
    RuntimeStatus status;
    status.driverReady = true;
    status.legacy32Exists = std::filesystem::exists(Legacy32Path());
    status.legacy64Exists = std::filesystem::exists(Legacy64Path());
    status.embeddedInfVariant0Exists = EmbeddedInfVariantExists(0);
    status.embeddedInfVariant1Exists = EmbeddedInfVariantExists(1);
    status.infKeyboardServiceInstalled = ServiceExists(kInfKeyboardServiceName);
    status.infMouseServiceInstalled = ServiceExists(kInfMouseServiceName);
    status.infKeyboardSysInstalled = std::filesystem::exists(SystemDriversDir() / kInfKeyboardSysName);
    status.infMouseSysInstalled = std::filesystem::exists(SystemDriversDir() / kInfMouseSysName);
    const InfDeviceState keyDevice = QueryInfDeviceState(kInfKeyboardInstanceId, kInfKeyboardServiceName);
    const InfDeviceState mouseDevice = QueryInfDeviceState(kInfMouseInstanceId, kInfMouseServiceName);
    status.infKeyboardDeviceStarted = keyDevice.started;
    status.infMouseDeviceStarted = mouseDevice.started;
    status.infKeyboardDeviceBound = keyDevice.boundToService;
    status.infMouseDeviceBound = mouseDevice.boundToService;

    std::wstringstream message;
    message << L"DD runtime ready. source=" << source << L"\n"
            << L"DD64.dll=" << (status.legacy64Exists ? L"present" : L"missing") << L"\n"
            << L"DD32.dll=" << (status.legacy32Exists ? L"present" : L"missing") << L"\n"
            << L"embedded_inf_variant_1=" << (status.embeddedInfVariant1Exists ? L"present" : L"missing") << L"\n"
            << L"embedded_inf_variant_0=" << (status.embeddedInfVariant0Exists ? L"present" : L"missing") << L"\n"
            << L"service_key=" << (status.infKeyboardServiceInstalled ? L"present" : L"missing") << L"\n"
            << L"service_mouse=" << (status.infMouseServiceInstalled ? L"present" : L"missing") << L"\n"
            << L"sys_key=" << (status.infKeyboardSysInstalled ? L"present" : L"missing") << L"\n"
            << L"sys_mouse=" << (status.infMouseSysInstalled ? L"present" : L"missing") << L"\n"
            << L"device_key_started=" << (status.infKeyboardDeviceStarted ? L"yes" : L"no") << L"\n"
            << L"device_mouse_started=" << (status.infMouseDeviceStarted ? L"yes" : L"no") << L"\n"
            << L"device_key_bound=" << (status.infKeyboardDeviceBound ? L"yes" : L"no") << L"\n"
            << L"device_mouse_bound=" << (status.infMouseDeviceBound ? L"yes" : L"no") << L"\n";
    AppendLog(logsDir_, kInfInstallLogName, message.str());
}

bool DdRuntimeService::InfDevicesReady() const {
    const InfDeviceState keyDevice = QueryInfDeviceState(kInfKeyboardInstanceId, kInfKeyboardServiceName);
    const InfDeviceState mouseDevice = QueryInfDeviceState(kInfMouseInstanceId, kInfMouseServiceName);
    return keyDevice.started && keyDevice.boundToService &&
           mouseDevice.started && mouseDevice.boundToService;
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

bool DdRuntimeService::EmbeddedInfVariantExists(int variant) const {
    const QString variantPath = QStringLiteral(":/dd/inf/%1/dd.key.96688.inf").arg(variant);
    return QFile::exists(variantPath);
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
