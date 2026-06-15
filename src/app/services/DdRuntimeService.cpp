#include "services/DdRuntimeService.h"

#include <Windows.h>
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
    status.legacy32Exists = std::filesystem::exists(Legacy32Path());
    status.legacy64Exists = std::filesystem::exists(Legacy64Path());
    status.embeddedInfVariant0Exists = EmbeddedInfVariantExists(0);
    status.embeddedInfVariant1Exists = EmbeddedInfVariantExists(1);
    status.infKeyboardServiceInstalled = ServiceExists(kInfKeyboardServiceName);
    status.infMouseServiceInstalled = ServiceExists(kInfMouseServiceName);
    status.infKeyboardSysInstalled = std::filesystem::exists(SystemDriversDir() / kInfKeyboardSysName);
    status.infMouseSysInstalled = std::filesystem::exists(SystemDriversDir() / kInfMouseSysName);
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

    return status;
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
