#include "services/DdRuntimeService.h"
#include "services/XiaojiRuntimeService.h"

#include <Windows.h>
#include <SetupAPI.h>
#include <cfgmgr32.h>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>
#include <QString>
#include <QTextStream>
#include <QStringConverter>

#include <algorithm>
#include <chrono>
#include <cwctype>
#include <fstream>
#include <functional>
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
constexpr std::uintmax_t kMaxDriverLogBytes = 1024 * 1024;

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

    const auto fullPath = logsDir / fileName;
    bool truncate = false;
    if (std::filesystem::exists(fullPath, error) && !error) {
        const auto currentSize = std::filesystem::file_size(fullPath, error);
        truncate = !error && currentSize > kMaxDriverLogBytes;
    }
    error.clear();

    const QString logPath = QString::fromStdWString(fullPath.wstring());
    QFile logFile(logPath);
    const auto openMode = QIODevice::WriteOnly |
        (truncate ? QIODevice::Truncate : QIODevice::Append) |
        QIODevice::Text;
    if (!logFile.open(openMode)) {
        return;
    }

    const auto now = std::chrono::system_clock::now();
    const std::time_t nowTime = std::chrono::system_clock::to_time_t(now);
    std::tm localTime{};
    localtime_s(&localTime, &nowTime);

    wchar_t timeBuffer[32] = {};
    if (std::wcsftime(timeBuffer, std::size(timeBuffer), L"%Y-%m-%d %H:%M:%S", &localTime) == 0) {
        return;
    }

    QTextStream stream(&logFile);
    stream.setEncoding(QStringConverter::Utf8);
    stream << "[" << QString::fromWCharArray(timeBuffer) << "]\n"
           << QString::fromStdWString(message) << "\n";
    stream.flush();
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

bool IsPublishedInfName(const std::wstring& value) {
    const std::wstring lowerValue = ToLower(value);
    return lowerValue.size() > 7 &&
           lowerValue.rfind(L"oem", 0) == 0 &&
           lowerValue.substr(lowerValue.size() - 4) == L".inf";
}

std::wstring TrimText(std::wstring text) {
    const auto first = text.find_first_not_of(L" \t\r\n");
    if (first == std::wstring::npos) {
        return {};
    }
    const auto last = text.find_last_not_of(L" \t\r\n");
    return text.substr(first, last - first + 1);
}

void AddUniquePublishedName(std::vector<std::wstring>* names, const std::wstring& name) {
    if (!names || name.empty()) {
        return;
    }
    const auto exists = std::find_if(names->begin(), names->end(), [&name](const std::wstring& existing) {
        return _wcsicmp(existing.c_str(), name.c_str()) == 0;
    });
    if (exists == names->end()) {
        names->push_back(name);
    }
}

std::vector<std::wstring> ExtractHidPublishedNamesFromPnputilOutput(const std::wstring& output) {
    std::vector<std::wstring> names;
    std::wistringstream input(output);
    std::wstring line;
    std::vector<std::wstring> blockValues;
    bool blockHasHidOriginalName = false;

    const auto flushBlock = [&]() {
        if (blockHasHidOriginalName) {
            for (const auto& value : blockValues) {
                if (IsPublishedInfName(value)) {
                    AddUniquePublishedName(&names, value);
                }
            }
        }
        blockValues.clear();
        blockHasHidOriginalName = false;
    };

    while (std::getline(input, line)) {
        line = TrimText(line);
        if (line.empty()) {
            flushBlock();
            continue;
        }

        const auto colon = line.find(L':');
        if (colon == std::wstring::npos) {
            continue;
        }

        const std::wstring value = TrimText(line.substr(colon + 1));
        const std::wstring lowerValue = ToLower(value);
        if (!value.empty()) {
            blockValues.push_back(value);
        }
        if (lowerValue == L"ddhid63340.inf") {
            blockHasHidOriginalName = true;
        }
    }
    flushBlock();
    return names;
}

std::vector<std::wstring> EnumerateHidDriverPublishedNames(const std::wstring& pnputilPath,
                                                           std::wstringstream* message,
                                                           bool* enumSucceeded = nullptr) {
    if (enumSucceeded) {
        *enumSucceeded = false;
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

    if (!enumOk) {
        if (message) {
            *message << L"HID enum driver packages -> failed (exit_code=" << enumExitCode << L")\n";
            if (!enumOutput.empty()) {
                *message << enumOutput << L"\n";
            }
            if (!enumError.empty()) {
                *message << enumError << L"\n";
            }
        }
        return {};
    }

    if (enumSucceeded) {
        *enumSucceeded = true;
    }
    auto publishedNames = ExtractHidPublishedNamesFromPnputilOutput(enumOutput);
    if (message) {
        *message << L"HID enum driver packages -> found " << publishedNames.size() << L"\n";
    }
    return publishedNames;
}

std::vector<std::wstring> ExtractPublishedNamesForOriginalInfs(const std::wstring& output,
                                                               const std::vector<std::wstring>& originalNames) {
    std::vector<std::wstring> names;
    std::wistringstream input(output);
    std::wstring line;
    std::vector<std::wstring> blockValues;
    bool blockMatches = false;

    const auto flushBlock = [&]() {
        if (blockMatches) {
            for (const auto& value : blockValues) {
                if (IsPublishedInfName(value)) {
                    AddUniquePublishedName(&names, value);
                }
            }
        }
        blockValues.clear();
        blockMatches = false;
    };

    while (std::getline(input, line)) {
        line = TrimText(line);
        if (line.empty()) {
            flushBlock();
            continue;
        }

        const auto colon = line.find(L':');
        if (colon == std::wstring::npos) {
            continue;
        }

        const std::wstring value = TrimText(line.substr(colon + 1));
        const std::wstring lowerValue = ToLower(value);
        if (!value.empty()) {
            blockValues.push_back(value);
        }
        for (const auto& originalName : originalNames) {
            if (lowerValue == ToLower(originalName)) {
                blockMatches = true;
                break;
            }
        }
    }
    flushBlock();
    return names;
}

std::vector<std::wstring> EnumerateInfDriverPublishedNames(const std::wstring& pnputilPath,
                                                           std::wstringstream* message,
                                                           bool* enumSucceeded = nullptr) {
    if (enumSucceeded) {
        *enumSucceeded = false;
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

    if (!enumOk) {
        if (message) {
            *message << L"DD enum driver packages -> failed (exit_code=" << enumExitCode << L")\n";
            if (!enumOutput.empty()) {
                *message << enumOutput << L"\n";
            }
            if (!enumError.empty()) {
                *message << enumError << L"\n";
            }
        }
        return {};
    }

    if (enumSucceeded) {
        *enumSucceeded = true;
    }
    auto publishedNames = ExtractPublishedNamesForOriginalInfs(
        enumOutput,
        {L"dd.key.96688.inf", L"dd.mou.96688.inf"});
    if (message) {
        *message << L"DD enum driver packages -> found " << publishedNames.size() << L"\n";
    }
    return publishedNames;
}


bool PnputilDeleteDriverSucceeded(bool processOk, DWORD exitCode, const std::wstring& output) {
    if (processOk || exitCode == ERROR_SUCCESS_REBOOT_REQUIRED) {
        return true;
    }

    const std::wstring lowerOutput = ToLower(output);
    if (ContainsText(lowerOutput, L"failed") ||
        ContainsText(lowerOutput, L"error") ||
        ContainsText(lowerOutput, L"failure")) {
        return false;
    }

    return ContainsText(lowerOutput, L"deleted successfully") ||
           ContainsText(lowerOutput, L"package deleted successfully") ||
           ContainsText(lowerOutput, L"删除成功");
}

bool ServiceDeleteAccepted(bool processOk, DWORD exitCode, const std::wstring& output) {
    if (processOk || exitCode == ERROR_SUCCESS_REBOOT_REQUIRED) {
        return true;
    }

    const std::wstring lowerOutput = ToLower(output);
    return ContainsText(lowerOutput, L"marked for deletion") ||
           ContainsText(lowerOutput, L"已标记为删除");
}

bool PnputilAddDriverSucceeded(bool processOk, DWORD exitCode, const std::wstring& output) {
    if (processOk || exitCode == ERROR_SUCCESS_REBOOT_REQUIRED) {
        return true;
    }

    const std::wstring lowerOutput = ToLower(output);
    if (ContainsText(lowerOutput, L"failed") ||
        ContainsText(lowerOutput, L"error") ||
        ContainsText(lowerOutput, L"failed to") ||
        ContainsText(lowerOutput, L"failure") ||
        ContainsText(lowerOutput, L"失败") ||
        ContainsText(lowerOutput, L"未能")) {
        return false;
    }

    return ContainsText(lowerOutput, L"driver package added successfully") ||
           ContainsText(lowerOutput, L"driver package installed successfully") ||
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

bool DeviceBoundToService(HDEVINFO deviceInfo, SP_DEVINFO_DATA* deviceData, const wchar_t* serviceName) {
    wchar_t serviceBuffer[256] = {};
    DWORD propertyType = 0;
    DWORD requiredSize = 0;
    const BOOL propertyOk = SetupDiGetDeviceRegistryPropertyW(
        deviceInfo,
        deviceData,
        SPDRP_SERVICE,
        &propertyType,
        reinterpret_cast<PBYTE>(serviceBuffer),
        sizeof(serviceBuffer),
        &requiredSize);

    return propertyOk &&
           propertyType == REG_SZ &&
           _wcsicmp(serviceBuffer, serviceName) == 0;
}

bool DeviceStarted(DEVINST devInst) {
    ULONG status = 0;
    ULONG problem = 0;
    const CONFIGRET statusResult = CM_Get_DevNode_Status(&status, &problem, devInst, 0);
    return statusResult == CR_SUCCESS &&
           (status & DN_STARTED) != 0 &&
           (status & DN_HAS_PROBLEM) == 0;
}

bool ServiceMarkedForDelete(const wchar_t* serviceName) {
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
        return false;
    }

    const DWORD error = GetLastError();
    CloseServiceHandle(scm);
    return error == ERROR_SERVICE_MARKED_FOR_DELETE;
}

bool RemoveFileNowOrOnReboot(const std::filesystem::path& path, std::wstring* detail) {
    if (detail) {
        detail->clear();
    }

    std::error_code error;
    if (!std::filesystem::exists(path, error) || error) {
        if (detail) {
            *detail = L"missing";
        }
        return true;
    }

    error.clear();
    if (std::filesystem::remove(path, error)) {
        if (detail) {
            *detail = L"deleted_now";
        }
        return true;
    }

    if (!error && !std::filesystem::exists(path)) {
        if (detail) {
            *detail = L"deleted_now";
        }
        return true;
    }

    if (::MoveFileExW(path.c_str(), nullptr, MOVEFILE_DELAY_UNTIL_REBOOT)) {
        if (detail) {
            *detail = L"scheduled_delete_on_reboot";
        }
        return true;
    }

    if (detail) {
        const DWORD moveError = ::GetLastError();
        *detail = L"delete_error=" + std::to_wstring(error.value()) +
                  L", movefileex=" + std::to_wstring(moveError);
    }
    return false;
}

std::vector<std::wstring> EnumerateDeviceInstanceIds(const wchar_t* instanceIdPrefix) {
    std::vector<std::wstring> ids;
    if (instanceIdPrefix == nullptr || *instanceIdPrefix == L'\0') {
        return ids;
    }

    ULONG deviceIdListSize = 0;
    CONFIGRET result = CM_Get_Device_ID_List_SizeW(&deviceIdListSize, nullptr, CM_GETIDLIST_FILTER_NONE);
    if (result != CR_SUCCESS || deviceIdListSize == 0) {
        return ids;
    }

    std::vector<wchar_t> deviceIds(deviceIdListSize);
    result = CM_Get_Device_ID_ListW(nullptr, deviceIds.data(), deviceIdListSize, CM_GETIDLIST_FILTER_NONE);
    if (result != CR_SUCCESS) {
        return ids;
    }

    const std::wstring prefix(instanceIdPrefix);
    for (const wchar_t* id = deviceIds.data(); id != nullptr && *id != L'\0'; id += wcslen(id) + 1) {
        if (_wcsnicmp(id, prefix.c_str(), prefix.size()) == 0) {
            ids.emplace_back(id);
        }
    }
    return ids;
}

std::vector<std::wstring> EnumerateDeviceInstanceIdsFromSetupApi(const wchar_t* instanceIdPrefix) {
    std::vector<std::wstring> ids;
    if (instanceIdPrefix == nullptr || *instanceIdPrefix == L'\0') {
        return ids;
    }

    HDEVINFO deviceInfo = SetupDiGetClassDevsW(nullptr, nullptr, nullptr, DIGCF_ALLCLASSES);
    if (deviceInfo == INVALID_HANDLE_VALUE) {
        return ids;
    }

    const std::wstring prefix(instanceIdPrefix);
    for (DWORD index = 0;; ++index) {
        SP_DEVINFO_DATA deviceData{};
        deviceData.cbSize = sizeof(deviceData);
        if (!SetupDiEnumDeviceInfo(deviceInfo, index, &deviceData)) {
            break;
        }

        wchar_t instanceId[MAX_DEVICE_ID_LEN] = {};
        if (CM_Get_Device_IDW(deviceData.DevInst, instanceId, MAX_DEVICE_ID_LEN, 0) != CR_SUCCESS) {
            continue;
        }
        if (_wcsnicmp(instanceId, prefix.c_str(), prefix.size()) == 0) {
            ids.emplace_back(instanceId);
        }
    }

    SetupDiDestroyDeviceInfoList(deviceInfo);
    return ids;
}

std::wstring FormatConfigRet(CONFIGRET result) {
    return std::to_wstring(static_cast<unsigned long>(result));
}

std::wstring QueryDeviceRegistryStringProperty(HDEVINFO deviceInfo,
                                               SP_DEVINFO_DATA* deviceData,
                                               DWORD property) {
    wchar_t buffer[512] = {};
    DWORD propertyType = 0;
    DWORD requiredSize = 0;
    const BOOL ok = SetupDiGetDeviceRegistryPropertyW(
        deviceInfo,
        deviceData,
        property,
        &propertyType,
        reinterpret_cast<PBYTE>(buffer),
        sizeof(buffer),
        &requiredSize);
    if (!ok || propertyType != REG_SZ) {
        return {};
    }
    return buffer;
}

std::vector<std::wstring> EnumerateHidRootDeviceInstanceIds() {
    std::vector<std::wstring> ids;
    HDEVINFO deviceInfo = SetupDiGetClassDevsW(nullptr, nullptr, nullptr, DIGCF_ALLCLASSES);
    if (deviceInfo == INVALID_HANDLE_VALUE) {
        return ids;
    }

    for (DWORD index = 0;; ++index) {
        SP_DEVINFO_DATA deviceData{};
        deviceData.cbSize = sizeof(deviceData);
        if (!SetupDiEnumDeviceInfo(deviceInfo, index, &deviceData)) {
            break;
        }

        wchar_t instanceId[MAX_DEVICE_ID_LEN] = {};
        if (CM_Get_Device_IDW(deviceData.DevInst, instanceId, MAX_DEVICE_ID_LEN, 0) != CR_SUCCESS) {
            continue;
        }

        if (_wcsnicmp(instanceId, kHidRootDevicePrefix, wcslen(kHidRootDevicePrefix)) != 0) {
            continue;
        }

        const std::wstring service = QueryDeviceRegistryStringProperty(deviceInfo, &deviceData, SPDRP_SERVICE);
        const std::wstring description = QueryDeviceRegistryStringProperty(deviceInfo, &deviceData, SPDRP_DEVICEDESC);
        const std::wstring manufacturer = QueryDeviceRegistryStringProperty(deviceInfo, &deviceData, SPDRP_MFG);
        if (_wcsicmp(service.c_str(), kHidServiceName) != 0 &&
            _wcsicmp(description.c_str(), L"ddhid63340") != 0 &&
            _wcsicmp(manufacturer.c_str(), L"www.ddxoft.com") != 0) {
            continue;
        }

        ids.emplace_back(instanceId);
    }

    SetupDiDestroyDeviceInfoList(deviceInfo);
    return ids;
}

bool RemoveDeviceInstanceBestEffort(const std::wstring& instanceId, std::wstring* detail) {
    if (detail) {
        detail->clear();
    }

    DEVINST devInst = 0;
    CONFIGRET locateResult = CM_Locate_DevNodeW(&devInst, const_cast<DEVINSTID_W>(instanceId.c_str()), CM_LOCATE_DEVNODE_PHANTOM);
    if (locateResult != CR_SUCCESS) {
        if (detail) {
            *detail = L"locate=" + FormatConfigRet(locateResult);
        }
        return false;
    }

    PNP_VETO_TYPE vetoType = PNP_VetoTypeUnknown;
    wchar_t vetoName[MAX_PATH] = {};
    CONFIGRET queryRemoveResult = CM_Query_And_Remove_SubTreeW(
        devInst,
        &vetoType,
        vetoName,
        MAX_PATH,
        CM_REMOVE_NO_RESTART);
    if (queryRemoveResult == CR_SUCCESS) {
        if (detail) {
            *detail = L"query_remove=ok";
        }
        return true;
    }

    if (CM_Uninstall_DevNode(devInst, 0) == CR_SUCCESS) {
        if (detail) {
            *detail = L"uninstall=ok";
        }
        return true;
    }

    if (CM_Remove_SubTree(devInst, 0) == CR_SUCCESS) {
        if (detail) {
            *detail = L"remove_subtree=ok";
        }
        return true;
    }

    HDEVINFO deviceInfo = SetupDiCreateDeviceInfoList(nullptr, nullptr);
    if (deviceInfo != INVALID_HANDLE_VALUE) {
        SP_DEVINFO_DATA deviceData{};
        deviceData.cbSize = sizeof(deviceData);
        if (SetupDiOpenDeviceInfoW(deviceInfo, instanceId.c_str(), nullptr, 0, &deviceData)) {
            SP_REMOVEDEVICE_PARAMS removeParams{};
            removeParams.ClassInstallHeader.cbSize = sizeof(SP_CLASSINSTALL_HEADER);
            removeParams.ClassInstallHeader.InstallFunction = DIF_REMOVE;
            removeParams.Scope = DI_REMOVEDEVICE_GLOBAL;
            removeParams.HwProfile = 0;

            if (SetupDiSetClassInstallParamsW(
                    deviceInfo,
                    &deviceData,
                    &removeParams.ClassInstallHeader,
                    sizeof(removeParams)) &&
                SetupDiCallClassInstaller(DIF_REMOVE, deviceInfo, &deviceData)) {
                SetupDiDestroyDeviceInfoList(deviceInfo);
                if (detail) {
                    *detail = L"setupapi_remove=ok";
                }
                return true;
            }
        }
        SetupDiDestroyDeviceInfoList(deviceInfo);
    }

    if (detail) {
        *detail = L"query_remove=" + FormatConfigRet(queryRemoveResult);
        if (vetoType != PNP_VetoTypeUnknown && vetoName[0] != L'\0') {
            *detail += L", veto=" + std::to_wstring(static_cast<unsigned long>(vetoType)) + L":" + vetoName;
        }
    }
    return false;
}

InfDeviceState QueryInfDeviceStateByService(const wchar_t* serviceName) {
    InfDeviceState state;
    HDEVINFO deviceInfo = SetupDiGetClassDevsW(nullptr, nullptr, nullptr, DIGCF_ALLCLASSES | DIGCF_PRESENT);
    if (deviceInfo == INVALID_HANDLE_VALUE) {
        return state;
    }

    for (DWORD index = 0;; ++index) {
        SP_DEVINFO_DATA deviceData{};
        deviceData.cbSize = sizeof(deviceData);
        if (!SetupDiEnumDeviceInfo(deviceInfo, index, &deviceData)) {
            break;
        }

        if (!DeviceBoundToService(deviceInfo, &deviceData, serviceName)) {
            continue;
        }

        state.present = true;
        state.boundToService = true;
        if (DeviceStarted(deviceData.DevInst)) {
            state.started = true;
            break;
        }
    }

    SetupDiDestroyDeviceInfoList(deviceInfo);
    return state;
}

std::vector<std::wstring> EnumerateDeviceInstanceIdsByService(const wchar_t* serviceName) {
    std::vector<std::wstring> ids;
    if (serviceName == nullptr || *serviceName == L'\0') {
        return ids;
    }

    HDEVINFO deviceInfo = SetupDiGetClassDevsW(nullptr, nullptr, nullptr, DIGCF_ALLCLASSES);
    if (deviceInfo == INVALID_HANDLE_VALUE) {
        return ids;
    }

    for (DWORD index = 0;; ++index) {
        SP_DEVINFO_DATA deviceData{};
        deviceData.cbSize = sizeof(deviceData);
        if (!SetupDiEnumDeviceInfo(deviceInfo, index, &deviceData)) {
            break;
        }

        if (!DeviceBoundToService(deviceInfo, &deviceData, serviceName)) {
            continue;
        }

        wchar_t instanceId[MAX_DEVICE_ID_LEN] = {};
        if (CM_Get_Device_IDW(deviceData.DevInst, instanceId, MAX_DEVICE_ID_LEN, 0) == CR_SUCCESS) {
            ids.emplace_back(instanceId);
        }
    }

    SetupDiDestroyDeviceInfoList(deviceInfo);
    return ids;
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

bool ExtractInfVariant(const QString& payloadRoot, int variant, const QString& uniqueToken, InfVariantPaths* paths, std::wstring* message) {
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
    const QString prefix = QDir(payloadRoot).filePath(variantName) + QLatin1Char('/');
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

bool InstallInfVariant(const QString& payloadRoot, int variant,
                       const std::wstring& pnputilPath,
                       const QString& uniqueToken,
                       bool removeExistingDevices,
                       std::wstringstream* message) {
    InfVariantPaths paths;
    std::wstring extractMessage;
    if (!ExtractInfVariant(payloadRoot, variant, uniqueToken, &paths, &extractMessage)) {
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
        const bool removeKeyOk = RunProcess(
            paths.dcPath.toStdWString(),
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
        const bool removeMouOk = RunProcess(
            paths.dcPath.toStdWString(),
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
    const bool addKeyProcessOk = RunProcess(
        pnputilPath,
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
    const bool addMouProcessOk = RunProcess(
        pnputilPath,
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
    const bool updateKeyProcessOk = RunProcess(
        paths.dcPath.toStdWString(),
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
    const bool updateMouProcessOk = RunProcess(
        paths.dcPath.toStdWString(),
        {L"update", paths.mouInfPath.toStdWString(), L"dd.mou.96688"},
        &exitCode,
        &stepOutput,
        &stepError);
    const bool updateMouOk = DevconDeviceCommandSucceeded(updateMouProcessOk, exitCode);
    if (message) {
        *message << L"variant " << variant << L": update mouse device -> "
                 << FormatStepResult(updateMouOk, updateMouProcessOk, exitCode, stepOutput, stepError) << L"\n";
    }

    bool installKeyAccepted = false;
    if (!updateKeyOk) {
        stepOutput.clear();
        stepError.clear();
        exitCode = 1;
        const bool installKeyOk = RunProcess(
            paths.dcPath.toStdWString(),
            {L"install", paths.keyInfPath.toStdWString(), L"dd.key.96688"},
            &exitCode,
            &stepOutput,
            &stepError);
        installKeyAccepted = DevconDeviceCommandSucceeded(installKeyOk, exitCode);
        if (message) {
            *message << L"variant " << variant << L": create key device -> "
                     << FormatStepResult(installKeyAccepted, installKeyOk, exitCode, stepOutput, stepError) << L"\n";
        }
    } else if (message) {
        *message << L"variant " << variant << L": create key device -> skipped (update succeeded)\n";
    }

    bool installMouAccepted = false;
    if (!updateMouOk) {
        stepOutput.clear();
        stepError.clear();
        exitCode = 1;
        const bool installMouOk = RunProcess(
            paths.dcPath.toStdWString(),
            {L"install", paths.mouInfPath.toStdWString(), L"dd.mou.96688"},
            &exitCode,
            &stepOutput,
            &stepError);
        installMouAccepted = DevconDeviceCommandSucceeded(installMouOk, exitCode);
        if (message) {
            *message << L"variant " << variant << L": create mouse device -> "
                     << FormatStepResult(installMouAccepted, installMouOk, exitCode, stepOutput, stepError) << L"\n";
        }
    } else if (message) {
        *message << L"variant " << variant << L": create mouse device -> skipped (update succeeded)\n";
    }

    stepOutput.clear();
    stepError.clear();
    exitCode = 1;
    const bool enableKeyProcessOk = RunProcess(
        paths.dcPath.toStdWString(),
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
    const bool enableMouProcessOk = RunProcess(
        paths.dcPath.toStdWString(),
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
           ((updateKeyOk && updateMouOk) ||
            (installKeyAccepted && installMouAccepted) ||
            (enableKeyOk && enableMouOk));
}

}  // namespace

DdRuntimeService::DdRuntimeService(std::filesystem::path exeDir, std::filesystem::path logsDir)
    : exeDir_(std::move(exeDir)),
      logsDir_(std::move(logsDir)) {}

DdRuntimeService::RuntimeStatus DdRuntimeService::Inspect(bool driverReady, const std::wstring& lastError) const {
    RuntimeStatus status;
    status.driverReady = driverReady;
    status.lastError = lastError;
    status.driverRootPath = DriverRootDir().wstring();
    status.driverRootExists = QFileInfo(QString::fromStdWString(status.driverRootPath)).isDir();
    const auto xiaojiStatus = XiaojiRuntimeService(xiaojiDirectory_.empty() ? exeDir_ : xiaojiDirectory_).Inspect();
    status.xiaojiManagerExists = xiaojiStatus.managerExists;
    status.xiaojiInstallerExists = xiaojiStatus.installerExists;
    status.xiaojiKeyboardPresent = xiaojiStatus.keyboardPresent;
    status.xiaojiMousePresent = xiaojiStatus.mousePresent;
    status.xiaojiKeyboardReady = xiaojiStatus.keyboardReady;
    status.xiaojiMouseReady = xiaojiStatus.mouseReady;
    status.xiaojiProfileReady = xiaojiStatus.profileReady;
    status.xiaojiBoundPackagePresent = xiaojiStatus.boundPackagePresent;
    status.xiaojiConfigurationPresent = xiaojiStatus.configurationPresent;
    status.xiaojiError = xiaojiStatus.error;
    status.hidServiceInstalled = ServiceExists(kHidServiceName);
    status.hidServicePendingDelete = ServiceMarkedForDelete(kHidServiceName);
    status.hidSysInstalled = std::filesystem::exists(SystemDriversDir() / kHidSysName);
    status.hidDriverStorePresent = HidDriverStorePresent();
    status.hidRootDeviceOk = HidRootDeviceOk();
    status.hidChildDeviceOk = HidChildDeviceOk();
    status.hidDevicesPresent = !EnumerateHidRootDeviceInstanceIds().empty() ||
        !EnumerateDeviceInstanceIdsFromSetupApi(kHidChildDevicePrefix).empty();
    status.legacy32Exists = QFileInfo(QString::fromStdWString(Legacy32Path().wstring())).isFile();
    status.legacy64Exists = QFileInfo(QString::fromStdWString(Legacy64Path().wstring())).isFile();
#if defined(_WIN64)
    auto legacyRuntimePath = Legacy64Path();
    status.legacyRuntimeExists = status.legacy64Exists;
#else
    auto legacyRuntimePath = Legacy32Path();
    status.legacyRuntimeExists = status.legacy32Exists;
#endif
    // The loader also accepts the matching runtime beside the executable.
    if (!status.legacyRuntimeExists) {
        const auto fallbackPath = exeDir_ / legacyRuntimePath.filename();
        if (QFileInfo(QString::fromStdWString(fallbackPath.wstring())).isFile()) {
            legacyRuntimePath = fallbackPath;
            status.legacyRuntimeExists = true;
        }
    }
    status.legacyRuntimePath = legacyRuntimePath.wstring();
    status.embeddedInfVariant0Exists = EmbeddedInfVariantExists(0);
    status.embeddedInfVariant1Exists = EmbeddedInfVariantExists(1);
    status.infKeyboardServiceInstalled = ServiceExists(kInfKeyboardServiceName);
    status.infMouseServiceInstalled = ServiceExists(kInfMouseServiceName);
    status.infKeyboardServicePendingDelete = ServiceMarkedForDelete(kInfKeyboardServiceName);
    status.infMouseServicePendingDelete = ServiceMarkedForDelete(kInfMouseServiceName);
    status.infKeyboardSysInstalled = std::filesystem::exists(SystemDriversDir() / kInfKeyboardSysName);
    status.infMouseSysInstalled = std::filesystem::exists(SystemDriversDir() / kInfMouseSysName);

    const InfDeviceState keyDevice = QueryInfDeviceStateByService(kInfKeyboardServiceName);
    const InfDeviceState mouseDevice = QueryInfDeviceStateByService(kInfMouseServiceName);
    status.infKeyboardDeviceStarted = keyDevice.started;
    status.infMouseDeviceStarted = mouseDevice.started;
    status.infKeyboardDeviceBound = keyDevice.boundToService;
    status.infMouseDeviceBound = mouseDevice.boundToService;
    status.candidateDlls = {
        Legacy64Path().wstring(),
        Legacy32Path().wstring(),
    };

    if (!status.driverReady) {
        status.notes.push_back(L"Selected input driver is not ready. Input simulation will fail until the selected driver is available.");
    }
    if (!status.legacyRuntimeExists) {
        status.notes.push_back(L"The optional DD plugin runtime is not installed for this process architecture.");
    }
    if (!status.embeddedInfVariant0Exists && !status.embeddedInfVariant1Exists) {
        status.notes.push_back(L"No DD plugin driver payload was found; download the optional DD plugin to install or repair it.");
    }
    if (!status.infKeyboardDeviceStarted || !status.infMouseDeviceStarted ||
        !status.infKeyboardDeviceBound || !status.infMouseDeviceBound) {
        status.notes.push_back(L"DD INF device instances are not fully started and bound to their services.");
    }

    return status;
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
    bool initialEnumSucceeded = false;
    std::vector<std::wstring> publishedNames = EnumerateHidDriverPublishedNames(
        pnputilPath,
        &message,
        &initialEnumSucceeded);

    if (initialEnumSucceeded &&
        publishedNames.empty() &&
        !HidRootDeviceOk() && !HidChildDeviceOk() &&
        !ServiceExists(kHidServiceName) &&
        !std::filesystem::exists(SystemDriversDir() / kHidSysName) &&
        !HidDriverStorePresent() &&
        EnumerateHidRootDeviceInstanceIds().empty() &&
        EnumerateDeviceInstanceIdsFromSetupApi(kHidChildDevicePrefix).empty()) {
        const std::wstring summary = L"HID uninstall skipped: runtime and driver package are already removed.\n";
        if (resultMessage) {
            *resultMessage = summary;
        }
        AppendLog(logsDir_, L"dd-hid-uninstall.log", summary);
        return true;
    }

    message << L"HID remove device -> skipped (use system uninstall path)\n";

    bool removedPackage = initialEnumSucceeded;
    bool packageRemovalNeedsReboot = false;
    for (const auto& publishedName : publishedNames) {
        DWORD exitCode = 1;
        std::wstring stepOutput;
        std::wstring stepError;
        const bool deleteProcessOk = RunProcess(
            pnputilPath,
            {L"/delete-driver", publishedName, L"/uninstall", L"/force"},
            &exitCode,
            &stepOutput,
            &stepError,
            {},
            30000);
        if (exitCode == ERROR_SUCCESS_REBOOT_REQUIRED) {
            packageRemovalNeedsReboot = true;
        }
        const bool deleted = PnputilDeleteDriverSucceeded(deleteProcessOk, exitCode, stepOutput);
        message << L"HID delete driver " << publishedName << L" -> "
                << FormatStepResult(deleted, deleteProcessOk, exitCode, stepOutput, stepError) << L"\n";
        removedPackage = removedPackage && deleted;
    }

    bool finalEnumSucceeded = false;
    const auto remainingPublishedNames = EnumerateHidDriverPublishedNames(
        pnputilPath,
        &message,
        &finalEnumSucceeded);
    removedPackage = removedPackage && finalEnumSucceeded && remainingPublishedNames.empty();

    if (packageRemovalNeedsReboot && removedPackage) {
        message << L"HID package removal requires reboot; continuing best-effort cleanup of stale nodes.\n";
    }

    DWORD scExitCode = 1;
    std::wstring scOutput;
    std::wstring scError;
    const bool serviceDeleteProcessOk = RunProcess(
        scPath,
        {L"delete", kHidServiceName},
        &scExitCode,
        &scOutput,
        &scError,
        {},
        10000);
    const bool serviceDeleted = ServiceDeleteAccepted(serviceDeleteProcessOk, scExitCode, scOutput + L"\n" + scError);
    message << L"HID delete service -> "
            << FormatStepResult(serviceDeleted, serviceDeleteProcessOk, scExitCode, scOutput, scError) << L"\n";

    if (scExitCode == ERROR_SERVICE_MARKED_FOR_DELETE &&
        removedPackage &&
        EnumerateHidRootDeviceInstanceIds().empty() &&
        EnumerateDeviceInstanceIdsFromSetupApi(kHidChildDevicePrefix).empty() &&
        !HidRootDeviceOk() &&
        !HidChildDeviceOk() &&
        !std::filesystem::exists(SystemDriversDir() / kHidSysName) &&
        !HidDriverStorePresent()) {
        std::wstringstream summary;
        summary << L"HID \u5378\u8f7d\u5df2\u63d0\u4ea4\uff0c\u9700\u8981\u91cd\u542f\u5b8c\u6210\u3002\n";
        summary << L"\u9a71\u52a8\u5305\u3001SYS \u548c\u8bbe\u5907\u8282\u70b9\u5df2\u79fb\u9664\uff0c\u670d\u52a1\u5df2\u6807\u8bb0\u4e3a\u5220\u9664\uff0c\u91cd\u542f\u540e Windows \u4f1a\u5b8c\u6210\u6700\u7ec8\u6e05\u7406\u3002\n";
        summary << L"\u8be6\u7ec6\u8fc7\u7a0b\u5df2\u5199\u5165\u65e5\u5fd7\u3002";
        if (resultMessage) {
            *resultMessage = summary.str();
        }
        AppendLog(logsDir_, L"dd-hid-uninstall.log", summary.str() + L"\n\n" + message.str());
        return true;
    }

    const auto sysPath = SystemDriversDir() / kHidSysName;
    std::wstring sysRemoveDetail;
    const bool sysRemoved = RemoveFileNowOrOnReboot(sysPath, &sysRemoveDetail);
    message << L"HID remove sys -> " << (sysRemoved ? L"ok" : L"failed");
    if (!sysRemoveDetail.empty()) {
        message << L" (" << sysRemoveDetail << L")";
    }
    message << L"\n";

    for (const auto& instanceId : EnumerateDeviceInstanceIdsFromSetupApi(kHidChildDevicePrefix)) {
        std::wstring removeDetail;
        const bool removed = RemoveDeviceInstanceBestEffort(instanceId, &removeDetail);
        message << L"HID remove child instance " << instanceId << L" -> "
                << (removed ? L"ok" : L"failed");
        if (!removeDetail.empty()) {
            message << L" (" << removeDetail << L")";
        }
        message << L"\n";
    }

    for (const auto& instanceId : EnumerateHidRootDeviceInstanceIds()) {
        std::wstring removeDetail;
        const bool removed = RemoveDeviceInstanceBestEffort(instanceId, &removeDetail);
        message << L"HID remove root instance " << instanceId << L" -> "
                << (removed ? L"ok" : L"failed");
        if (!removeDetail.empty()) {
            message << L" (" << removeDetail << L")";
        }
        message << L"\n";
    }

    const bool stillHasRoot = !EnumerateHidRootDeviceInstanceIds().empty();
    const bool stillHasChild = !EnumerateDeviceInstanceIdsFromSetupApi(kHidChildDevicePrefix).empty();
    const bool stillHasService = ServiceExists(kHidServiceName);
    const bool stillHasSys = std::filesystem::exists(SystemDriversDir() / kHidSysName);
    const bool stillHasDriverStore = HidDriverStorePresent();
    const bool servicePendingDelete = serviceDeleted && stillHasService;
    const bool pendingRebootCleanup = packageRemovalNeedsReboot && removedPackage &&
        !stillHasSys &&
        !stillHasDriverStore;
    const bool pendingRebootOnly = pendingRebootCleanup &&
        !stillHasRoot &&
        !stillHasChild &&
        (!stillHasService || servicePendingDelete);
    const bool pendingSysFileCleanup = removedPackage &&
        !packageRemovalNeedsReboot &&
        !stillHasRoot &&
        !stillHasChild &&
        !stillHasService &&
        stillHasSys &&
        !stillHasDriverStore;
    const bool sysDeleteScheduled = sysRemoveDetail == L"scheduled_delete_on_reboot";
    const bool overallUninstalled = !stillHasRoot && !stillHasChild &&
        !stillHasService &&
        !stillHasSys &&
        !stillHasDriverStore &&
        removedPackage;
    message << L"HID final state -> root_device="
            << (stillHasRoot ? L"yes" : L"no")
            << L", child_device="
            << (stillHasChild ? L"yes" : L"no")
            << L", service="
            << (stillHasService ? L"yes" : L"no")
            << L", sys="
            << (stillHasSys ? L"yes" : L"no")
            << L", driver_store="
            << (stillHasDriverStore ? L"yes" : L"no")
            << L", service_pending_delete="
            << (servicePendingDelete ? L"yes" : L"no")
            << L", package_removed="
            << (removedPackage ? L"yes" : L"no")
            << L", package_removal_needs_reboot="
            << (packageRemovalNeedsReboot ? L"yes" : L"no")
            << L", remaining_packages="
            << remainingPublishedNames.size()
            << L"\n";

    std::wstringstream summary;
    summary << ((overallUninstalled || pendingRebootOnly || pendingSysFileCleanup)
        ? L"HID \u5378\u8f7d\u6210\u529f\u3002\n"
        : L"HID \u5378\u8f7d\u672a\u5b8c\u5168\u5b8c\u6210\u3002\n");
    if (pendingRebootCleanup) {
        summary << L"Windows \u5df2\u63a5\u53d7\u9a71\u52a8\u5305\u5378\u8f7d\u8bf7\u6c42\uff0c\u5e76\u8fd4\u56de 3010\uff08\u9700\u8981\u91cd\u542f\uff09\u3002\n";
        if (pendingRebootOnly) {
            summary << L"\u9a71\u52a8\u5305\u3001SYS \u548c\u8bbe\u5907\u8282\u70b9\u5df2\u6e05\u7406\uff0c\u670d\u52a1\u5df2\u6807\u8bb0\u4e3a\u5220\u9664\uff0c\u91cd\u542f\u540e Windows \u4f1a\u5b8c\u6210\u6700\u7ec8\u63d0\u4ea4\u3002\n";
        } else {
            summary << L"\u5df2\u7ee7\u7eed\u5c1d\u8bd5\u6e05\u7406\u6b8b\u7559\u670d\u52a1\u548c\u8bbe\u5907\u8282\u70b9\uff0c\u4ecd\u53ef\u80fd\u9700\u91cd\u542f\u540e\u91ca\u653e\u3002\n";
        }
    } else if (sysDeleteScheduled) {
        summary << L"\u9a71\u52a8\u5305\u3001\u670d\u52a1\u548c\u8bbe\u5907\u8282\u70b9\u5df2\u79fb\u9664\uff0cSYS \u6587\u4ef6\u5df2\u767b\u8bb0\u4e3a\u91cd\u542f\u65f6\u5f3a\u5236\u5220\u9664\u3002\n";
    } else if (pendingSysFileCleanup) {
        summary << L"\u9a71\u52a8\u5305\u3001\u670d\u52a1\u548c\u8bbe\u5907\u8282\u70b9\u5df2\u79fb\u9664\uff0c\u4ec5\u5269 SYS \u6587\u4ef6\u5f85 Windows \u5728\u91cd\u542f\u540e\u5b8c\u6210\u6e05\u7406\u3002\n";
    } else if (!stillHasRoot && !stillHasChild && !stillHasSys && !stillHasDriverStore &&
        !stillHasService && removedPackage) {
        summary << L"\u9a71\u52a8\u670d\u52a1\u3001\u9a71\u52a8\u5305\u548c\u8bbe\u5907\u8282\u70b9\u5df2\u79fb\u9664\u3002\n";
    } else if (!stillHasRoot && !stillHasChild && removedPackage) {
        summary << L"\u8bbe\u5907\u8282\u70b9\u5df2\u79fb\u9664\uff0c\u4f46 SYS \u6216 DriverStore \u4ecd\u53ef\u80fd\u6b8b\u7559\uff0c\u670d\u52a1\u72b6\u6001\u4e5f\u53ef\u80fd\u9700\u91cd\u542f\u540e\u91ca\u653e\u3002\n";
    } else {
        summary << L"\u7cfb\u7edf\u4e2d\u4ecd\u53ef\u80fd\u6b8b\u7559\u90e8\u5206\u9a71\u52a8\u5305\u3001\u8bbe\u5907\u8282\u70b9\u6216\u670d\u52a1\u72b6\u6001\u3002\n";
    }
    summary << L"\u5efa\u8bae\u91cd\u542f\u7cfb\u7edf\uff1b\u81f3\u5c11\u4e5f\u8bf7\u91cd\u542f\u8f6f\u4ef6\u540e\u518d\u7ee7\u7eed\u4f7f\u7528\u3002\n";
    summary << L"\u8be6\u7ec6\u8fc7\u7a0b\u5df2\u5199\u5165\u65e5\u5fd7\u3002";
    if (resultMessage) {
        *resultMessage = summary.str();
    }
    AppendLog(logsDir_, L"dd-hid-uninstall.log", summary.str() + L"\n\n" + message.str());
    return overallUninstalled || pendingRebootOnly || pendingSysFileCleanup || sysDeleteScheduled;
}


bool DdRuntimeService::UninstallEmbeddedInfDriver(std::wstring* resultMessage, bool* restartRequired) const {
    if (restartRequired) *restartRequired = false;
    const std::wstring pnputilPath = ResolvePnputilPath();
    if (pnputilPath.empty()) {
        if (resultMessage) {
            *resultMessage = L"Unable to resolve the Windows system directory.";
        }
        return false;
    }

    std::wstringstream message;
    bool enumSucceeded = false;
    auto publishedNames = EnumerateInfDriverPublishedNames(pnputilPath, &message, &enumSucceeded);

    bool packageDeletesAccepted = enumSucceeded;
    bool packageRemovalNeedsReboot = false;
    for (const auto& publishedName : publishedNames) {
        DWORD deleteExitCode = 1;
        std::wstring deleteOutput;
        std::wstring deleteError;
        const bool deleteProcessOk = RunProcess(
            pnputilPath,
            {L"/delete-driver", publishedName, L"/uninstall", L"/force"},
            &deleteExitCode,
            &deleteOutput,
            &deleteError,
            {},
            20000);
        const bool deleteAccepted = PnputilDeleteDriverSucceeded(deleteProcessOk, deleteExitCode, deleteOutput);
        packageDeletesAccepted = packageDeletesAccepted && deleteAccepted;
        packageRemovalNeedsReboot = packageRemovalNeedsReboot || deleteExitCode == ERROR_SUCCESS_REBOOT_REQUIRED;
        message << L"DD delete driver package " << publishedName << L" -> "
                << FormatStepResult(deleteAccepted, deleteProcessOk, deleteExitCode, deleteOutput, deleteError)
                << L"\n";
    }

    for (const auto* serviceName : {kInfKeyboardServiceName, kInfMouseServiceName}) {
        for (const auto& instanceId : EnumerateDeviceInstanceIdsByService(serviceName)) {
            std::wstring removeDetail;
            const bool removed = RemoveDeviceInstanceBestEffort(instanceId, &removeDetail);
            message << L"DD remove device " << instanceId << L" -> "
                    << (removed ? L"ok" : L"failed");
            if (!removeDetail.empty()) {
                message << L" (" << removeDetail << L")";
            }
            message << L"\n";
        }
    }

    bool serviceDeleteAccepted = true;
    for (const auto* serviceName : {kInfKeyboardServiceName, kInfMouseServiceName}) {
        DWORD serviceExitCode = 1;
        std::wstring serviceOutput;
        std::wstring serviceError;
        const bool serviceProcessOk = RunProcess(
            L"sc.exe",
            {L"delete", serviceName},
            &serviceExitCode,
            &serviceOutput,
            &serviceError,
            {},
            10000);
        const bool accepted = ServiceDeleteAccepted(serviceProcessOk, serviceExitCode, serviceOutput) ||
            !ServiceExists(serviceName);
        serviceDeleteAccepted = serviceDeleteAccepted && accepted;
        message << L"DD delete service " << serviceName << L" -> "
                << FormatStepResult(accepted, serviceProcessOk, serviceExitCode, serviceOutput, serviceError)
                << L"\n";
    }

    bool sysCleanupAccepted = true;
    bool sysCleanupNeedsReboot = false;
    for (const auto* sysName : {kInfKeyboardSysName, kInfMouseSysName}) {
        std::wstring removeDetail;
        const bool removed = RemoveFileNowOrOnReboot(SystemDriversDir() / sysName, &removeDetail);
        sysCleanupAccepted = sysCleanupAccepted && removed;
        sysCleanupNeedsReboot = sysCleanupNeedsReboot || removeDetail == L"scheduled_delete_on_reboot";
        message << L"DD remove sys " << sysName << L" -> " << (removed ? L"ok" : L"failed");
        if (!removeDetail.empty()) {
            message << L" (" << removeDetail << L")";
        }
        message << L"\n";
    }

    bool finalEnumSucceeded = false;
    auto remainingPublishedNames = EnumerateInfDriverPublishedNames(pnputilPath, &message, &finalEnumSucceeded);
    const bool stillHasKeyService = ServiceExists(kInfKeyboardServiceName);
    const bool stillHasMouseService = ServiceExists(kInfMouseServiceName);
    const bool stillHasKeySys = std::filesystem::exists(SystemDriversDir() / kInfKeyboardSysName);
    const bool stillHasMouseSys = std::filesystem::exists(SystemDriversDir() / kInfMouseSysName);
    const InfDeviceState keyDevice = QueryInfDeviceStateByService(kInfKeyboardServiceName);
    const InfDeviceState mouseDevice = QueryInfDeviceStateByService(kInfMouseServiceName);
    const bool keyServicePendingDelete = ServiceMarkedForDelete(kInfKeyboardServiceName);
    const bool mouseServicePendingDelete = ServiceMarkedForDelete(kInfMouseServiceName);

    const bool noDevices = !keyDevice.present && !mouseDevice.present;
    const bool noServices = (!stillHasKeyService || keyServicePendingDelete) &&
        (!stillHasMouseService || mouseServicePendingDelete);
    const bool noSys = !stillHasKeySys && !stillHasMouseSys;
    const bool noPackages = finalEnumSucceeded && remainingPublishedNames.empty();
    const bool overallUninstalled = noDevices && !stillHasKeyService && !stillHasMouseService && noSys && noPackages;
    const bool pendingRebootOnly = noDevices && noServices &&
        (noPackages || packageRemovalNeedsReboot) &&
        packageDeletesAccepted && serviceDeleteAccepted && sysCleanupAccepted &&
        (packageRemovalNeedsReboot || sysCleanupNeedsReboot || keyServicePendingDelete || mouseServicePendingDelete);
    if (restartRequired) *restartRequired = !overallUninstalled && pendingRebootOnly;

    message << L"DD final state -> key_device=" << (keyDevice.present ? L"yes" : L"no")
            << L", mouse_device=" << (mouseDevice.present ? L"yes" : L"no")
            << L", key_service=" << (stillHasKeyService ? L"yes" : L"no")
            << L", mouse_service=" << (stillHasMouseService ? L"yes" : L"no")
            << L", key_sys=" << (stillHasKeySys ? L"yes" : L"no")
            << L", mouse_sys=" << (stillHasMouseSys ? L"yes" : L"no")
            << L", key_service_pending_delete=" << (keyServicePendingDelete ? L"yes" : L"no")
            << L", mouse_service_pending_delete=" << (mouseServicePendingDelete ? L"yes" : L"no")
            << L", remaining_packages=" << remainingPublishedNames.size()
            << L"\n";

    std::wstringstream summary;
    summary << ((overallUninstalled || pendingRebootOnly)
        ? L"DD \u5378\u8f7d\u6210\u529f\u3002\n"
        : L"DD \u5378\u8f7d\u672a\u5b8c\u5168\u5b8c\u6210\u3002\n");
    if (overallUninstalled) {
        summary << L"DD \u9a71\u52a8\u5305\u3001\u670d\u52a1\u3001SYS \u548c\u8bbe\u5907\u8282\u70b9\u5df2\u6e05\u7406\u3002\n";
    } else if (pendingRebootOnly) {
        summary << L"Windows \u5df2\u63a5\u53d7 DD \u5378\u8f7d\u8bf7\u6c42\uff0c\u5269\u4f59\u72b6\u6001\u9700\u8981\u91cd\u542f\u540e\u91ca\u653e\u3002\n";
    } else {
        summary << L"\u7cfb\u7edf\u4e2d\u4ecd\u53ef\u80fd\u6b8b\u7559 DD \u9a71\u52a8\u5305\u3001\u670d\u52a1\u3001SYS \u6216\u8bbe\u5907\u8282\u70b9\u3002\n";
    }
    summary << L"\u5efa\u8bae\u91cd\u542f\u7cfb\u7edf\uff1b\u8be6\u7ec6\u8fc7\u7a0b\u5df2\u5199\u5165\u65e5\u5fd7\u3002";
    if (resultMessage) {
        *resultMessage = summary.str();
    }
    AppendLog(logsDir_, L"dd-inf-uninstall.log", summary.str() + L"\n\n" + message.str());
    return overallUninstalled || pendingRebootOnly;
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
            *resultMessage = QStringLiteral("DD 插件的安装载荷缺失，请先在插件中心下载或重新安装 DD 驱动插件。").toStdWString();
        }
        return false;
    }

    std::wstringstream message;
    bool overallInstalled = false;
    int installedVariant = -1;
    const QString uniqueToken = QString::fromStdWString(
        std::to_wstring(GetCurrentProcessId()) + L"-" +
        std::to_wstring(static_cast<unsigned long long>(GetTickCount64())));

    for (const int variant : variants) {
        InstallInfVariant(QString::fromStdWString((DriverRootDir() / L"inf").wstring()), variant, pnputilPath, uniqueToken, false, &message);

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

    std::wstringstream summary;
    if (overallInstalled) {
        summary << L"INF install succeeded. active_variant=" << installedVariant << L"\n";
    } else {
        summary << L"INF install did not complete successfully.\n";
    }
    summary << message.str();
    if (resultMessage) {
        *resultMessage = summary.str();
    }
    AppendLog(logsDir_, L"dd-inf-install.log", summary.str());
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
            *resultMessage = QStringLiteral("DD 插件的安装载荷缺失，请先在插件中心下载或重新安装 DD 驱动插件。").toStdWString();
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
    // 每次尝试用全新目录：dc64.exe 退出后可能仍被杀软或子进程短暂占用，
    // 复用同一目录会在重试时因无法覆盖文件而跳过真正的删设备重建步骤。
    int attempt = 0;
    const auto freshToken = [&attempt]() {
        return QString::fromStdWString(
            std::to_wstring(GetCurrentProcessId()) + L"-repair-" +
            std::to_wstring(static_cast<unsigned long long>(GetTickCount64())) +
            L"-" + std::to_wstring(++attempt));
    };

    for (const int variant : variants) {
        message << L"variant " << variant << L": repair begin (refresh existing devices)\n";
        InstallInfVariant(QString::fromStdWString((DriverRootDir() / L"inf").wstring()), variant, pnputilPath, freshToken(), false, &message);
        if (readinessProbe && readinessProbe()) {
            repaired = true;
            repairedVariant = variant;
            message << L"variant " << variant << L": readiness probe -> ok\n";
            break;
        }
        message << L"variant " << variant << L": readiness probe after refresh -> failed\n";

        message << L"variant " << variant << L": repair retry (remove stale devices)\n";
        InstallInfVariant(QString::fromStdWString((DriverRootDir() / L"inf").wstring()), variant, pnputilPath, freshToken(), true, &message);
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
    AppendLog(logsDir_, L"dd-inf-repair.log", summary.str());
    return repaired;
}

void DdRuntimeService::LogRuntimeError(const wchar_t* fileName, const std::wstring& message) const {
    AppendLog(logsDir_, fileName && *fileName ? fileName : L"dd-runtime.log", message);
}

bool DdRuntimeService::InfDevicesReady() const {
    const InfDeviceState keyDevice = QueryInfDeviceStateByService(kInfKeyboardServiceName);
    const InfDeviceState mouseDevice = QueryInfDeviceStateByService(kInfMouseServiceName);
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
    const auto pluginRoot = QString::fromStdWString((DriverRootDir() / L"inf").wstring());
    const auto directory = QDir(pluginRoot).filePath(QString::number(variant));
    for (const auto* name : {"dc64.exe", "dd.key.96688.inf", "dd.key.96688.cat", "dd.key.96688.sys", "dd.mou.96688.inf", "dd.mou.96688.cat", "dd.mou.96688.sys"})
        if (!QFile::exists(QDir(directory).filePath(QString::fromLatin1(name)))) return false;
    return true;
}

bool DdRuntimeService::HidRootDeviceOk() const {
    HDEVINFO deviceInfo = SetupDiGetClassDevsW(nullptr, nullptr, nullptr, DIGCF_ALLCLASSES | DIGCF_PRESENT);
    if (deviceInfo == INVALID_HANDLE_VALUE) {
        return false;
    }

    bool found = false;
    for (DWORD index = 0;; ++index) {
        SP_DEVINFO_DATA deviceData{};
        deviceData.cbSize = sizeof(deviceData);
        if (!SetupDiEnumDeviceInfo(deviceInfo, index, &deviceData)) {
            break;
        }

        wchar_t instanceId[MAX_DEVICE_ID_LEN] = {};
        if (CM_Get_Device_IDW(deviceData.DevInst, instanceId, MAX_DEVICE_ID_LEN, 0) != CR_SUCCESS) {
            continue;
        }

        if (_wcsnicmp(instanceId, kHidRootDevicePrefix, wcslen(kHidRootDevicePrefix)) != 0) {
            continue;
        }

        const std::wstring service = QueryDeviceRegistryStringProperty(deviceInfo, &deviceData, SPDRP_SERVICE);
        const std::wstring description = QueryDeviceRegistryStringProperty(deviceInfo, &deviceData, SPDRP_DEVICEDESC);
        const std::wstring manufacturer = QueryDeviceRegistryStringProperty(deviceInfo, &deviceData, SPDRP_MFG);
        if (_wcsicmp(service.c_str(), kHidServiceName) != 0 &&
            _wcsicmp(description.c_str(), L"ddhid63340") != 0 &&
            _wcsicmp(manufacturer.c_str(), L"www.ddxoft.com") != 0) {
            continue;
        }

        if (DeviceStarted(deviceData.DevInst)) {
            found = true;
            break;
        }
    }

    SetupDiDestroyDeviceInfoList(deviceInfo);
    return found;
}

bool DdRuntimeService::HidChildDeviceOk() const {
    return DeviceInstanceOk(kHidChildDevicePrefix);
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

bool DdRuntimeService::HidDriverStorePresent() const {
    wchar_t windowsDir[MAX_PATH] = {};
    const UINT length = GetWindowsDirectoryW(windowsDir, MAX_PATH);
    const std::filesystem::path base = (length == 0 || length >= MAX_PATH)
        ? std::filesystem::path(L"C:\\Windows")
        : std::filesystem::path(windowsDir);
    const auto fileRepository = base / L"System32" / L"DriverStore" / L"FileRepository";

    std::error_code error;
    if (!std::filesystem::exists(fileRepository, error) || error) {
        return false;
    }

    for (const auto& entry : std::filesystem::directory_iterator(fileRepository, error)) {
        if (error) {
            return false;
        }
        if (!entry.is_directory(error) || error) {
            continue;
        }

        const auto name = entry.path().filename().wstring();
        if (name.rfind(L"ddhid63340.inf_", 0) == 0) {
            return true;
        }
    }
    return false;
}

std::filesystem::path DdRuntimeService::Legacy32Path() const {
    return DriverRootDir() / L"DD32.dll";
}

std::filesystem::path DdRuntimeService::Legacy64Path() const {
    return DriverRootDir() / L"DD64.dll";
}
