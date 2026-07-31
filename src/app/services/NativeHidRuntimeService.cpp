#include "services/NativeHidRuntimeService.h"

#include "NativeHidDriver.h"
#include "XiaoJiVhidProtocol.h"

#include <Windows.h>
#include <SetupAPI.h>
#include <cfgmgr32.h>
#include <devguid.h>
#include <newdev.h>

#include <array>
#include <fstream>
#include <set>
#include <sstream>
#include <utility>
#include <vector>

namespace {

std::wstring FormatWin32Error(DWORD code) {
    wchar_t* buffer = nullptr;
    const DWORD length = ::FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, code, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<LPWSTR>(&buffer), 0, nullptr);
    std::wstring result = std::to_wstring(code);
    if (length > 0 && buffer) result += L" (" + std::wstring(buffer) + L")";
    if (buffer) ::LocalFree(buffer);
    return result;
}

void AppendLog(const std::filesystem::path& logsDir, const std::wstring& text) {
    std::error_code error;
    std::filesystem::create_directories(logsDir, error);
    std::wofstream stream(logsDir / L"native-hid.log", std::ios::app);
    if (stream) stream << text << L"\n";
}

bool ServiceExists() {
    SC_HANDLE manager = ::OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!manager) return false;
    SC_HANDLE service = ::OpenServiceW(manager, XIAOJI_VHID_SERVICE_NAME, SERVICE_QUERY_STATUS);
    if (service) ::CloseServiceHandle(service);
    ::CloseServiceHandle(manager);
    return service != nullptr;
}

std::filesystem::path SystemDriverPath() {
    std::array<wchar_t, MAX_PATH> windowsDir{};
    const UINT length = ::GetWindowsDirectoryW(windowsDir.data(), static_cast<UINT>(windowsDir.size()));
    const std::filesystem::path root = length > 0 && length < windowsDir.size()
        ? std::filesystem::path(windowsDir.data())
        : std::filesystem::path(L"C:\\Windows");
    return root / L"System32" / L"drivers" / L"XiaoJiVhid.sys";
}

bool MultiSzContains(const BYTE* buffer, DWORD byteCount, const wchar_t* expected) {
    if (!buffer || byteCount < sizeof(wchar_t) || !expected) return false;
    const wchar_t* current = reinterpret_cast<const wchar_t*>(buffer);
    const wchar_t* end = reinterpret_cast<const wchar_t*>(buffer + byteCount);
    while (current < end && *current) {
        if (_wcsicmp(current, expected) == 0) return true;
        current += wcslen(current) + 1;
    }
    return false;
}

std::wstring ReadPublishedInfName(HDEVINFO set, SP_DEVINFO_DATA& data) {
    wchar_t driverKey[MAX_PATH]{};
    DWORD propertyType = 0;
    if (!::SetupDiGetDeviceRegistryPropertyW(set, &data, SPDRP_DRIVER, &propertyType,
            reinterpret_cast<PBYTE>(driverKey), sizeof(driverKey), nullptr)) {
        return {};
    }

    const std::wstring path = L"SYSTEM\\CurrentControlSet\\Control\\Class\\" + std::wstring(driverKey);
    HKEY key = nullptr;
    if (::RegOpenKeyExW(HKEY_LOCAL_MACHINE, path.c_str(), 0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS) {
        return {};
    }
    wchar_t infName[MAX_PATH]{};
    DWORD type = 0;
    DWORD size = sizeof(infName);
    const LONG result = ::RegQueryValueExW(key, L"InfPath", nullptr, &type,
        reinterpret_cast<BYTE*>(infName), &size);
    ::RegCloseKey(key);
    return result == ERROR_SUCCESS && type == REG_SZ ? std::wstring(infName) : std::wstring{};
}

std::wstring PublishedInfNameForPackage(const std::filesystem::path& infPath) {
    DWORD required = 0;
    ::SetupGetInfDriverStoreLocationW(infPath.c_str(), nullptr, nullptr, nullptr, 0, &required);
    if (required == 0) return {};

    std::vector<wchar_t> storeLocation(required);
    if (!::SetupGetInfDriverStoreLocationW(infPath.c_str(), nullptr, nullptr,
            storeLocation.data(), static_cast<DWORD>(storeLocation.size()), nullptr)) {
        return {};
    }

    required = 0;
    ::SetupGetInfPublishedNameW(storeLocation.data(), nullptr, 0, &required);
    if (required == 0) return {};

    std::vector<wchar_t> publishedName(required);
    if (!::SetupGetInfPublishedNameW(storeLocation.data(), publishedName.data(),
            static_cast<DWORD>(publishedName.size()), nullptr)) {
        return {};
    }
    return std::filesystem::path(publishedName.data()).filename().wstring();
}

struct DeviceEnumerationResult {
    bool present = false;
    bool started = false;
    std::set<std::wstring> publishedInfNames;
};

DeviceEnumerationResult EnumerateDevices(bool removeDevices, std::wstringstream* log) {
    DeviceEnumerationResult result;
    HDEVINFO set = ::SetupDiGetClassDevsW(nullptr, nullptr, nullptr, DIGCF_ALLCLASSES);
    if (set == INVALID_HANDLE_VALUE) return result;

    for (DWORD index = 0;; ++index) {
        SP_DEVINFO_DATA data{};
        data.cbSize = sizeof(data);
        if (!::SetupDiEnumDeviceInfo(set, index, &data)) break;

        DWORD required = 0;
        DWORD type = 0;
        ::SetupDiGetDeviceRegistryPropertyW(set, &data, SPDRP_HARDWAREID, &type,
            nullptr, 0, &required);
        if (required == 0) continue;
        std::vector<BYTE> ids(required + sizeof(wchar_t));
        if (!::SetupDiGetDeviceRegistryPropertyW(set, &data, SPDRP_HARDWAREID, &type,
                ids.data(), static_cast<DWORD>(ids.size()), nullptr) ||
            !MultiSzContains(ids.data(), required, XIAOJI_VHID_HARDWARE_ID)) {
            continue;
        }

        result.present = true;
        ULONG status = 0;
        ULONG problem = 0;
        if (::CM_Get_DevNode_Status(&status, &problem, data.DevInst, 0) == CR_SUCCESS &&
            problem == 0 && (status & DN_STARTED) != 0) {
            result.started = true;
        }
        const std::wstring infName = ReadPublishedInfName(set, data);
        if (!infName.empty()) result.publishedInfNames.insert(infName);

        if (removeDevices) {
            SP_REMOVEDEVICE_PARAMS params{};
            params.ClassInstallHeader.cbSize = sizeof(SP_CLASSINSTALL_HEADER);
            params.ClassInstallHeader.InstallFunction = DIF_REMOVE;
            params.Scope = DI_REMOVEDEVICE_GLOBAL;
            params.HwProfile = 0;
            const bool removed = ::SetupDiSetClassInstallParamsW(
                set, &data, &params.ClassInstallHeader, sizeof(params)) &&
                ::SetupDiCallClassInstaller(DIF_REMOVE, set, &data);
            if (log) *log << L"Remove device -> " << (removed ? L"ok" : FormatWin32Error(::GetLastError())) << L"\n";
        }
    }

    ::SetupDiDestroyDeviceInfoList(set);
    return result;
}

bool CreateRootDevice(std::wstringstream* log) {
    HDEVINFO set = ::SetupDiCreateDeviceInfoList(&GUID_DEVCLASS_HIDCLASS, nullptr);
    if (set == INVALID_HANDLE_VALUE) {
        if (log) *log << L"SetupDiCreateDeviceInfoList failed: " << FormatWin32Error(::GetLastError()) << L"\n";
        return false;
    }

    SP_DEVINFO_DATA data{};
    data.cbSize = sizeof(data);
    bool ok = ::SetupDiCreateDeviceInfoW(set, L"XiaoJiVhid", &GUID_DEVCLASS_HIDCLASS,
        L"XiaoJi HID Input Device", nullptr, DICD_GENERATE_ID, &data) != FALSE;
    if (ok) {
        const wchar_t hardwareIds[] = XIAOJI_VHID_HARDWARE_ID L"\0";
        ok = ::SetupDiSetDeviceRegistryPropertyW(set, &data, SPDRP_HARDWAREID,
            reinterpret_cast<const BYTE*>(hardwareIds), sizeof(hardwareIds)) != FALSE;
    }
    if (ok) ok = ::SetupDiCallClassInstaller(DIF_REGISTERDEVICE, set, &data) != FALSE;
    if (!ok && log) *log << L"Register root device failed: " << FormatWin32Error(::GetLastError()) << L"\n";
    ::SetupDiDestroyDeviceInfoList(set);
    return ok;
}

}  // namespace

NativeHidRuntimeService::NativeHidRuntimeService(
    std::filesystem::path exeDir, std::filesystem::path logsDir)
    : exeDir_(std::move(exeDir)), logsDir_(std::move(logsDir)) {}

NativeHidRuntimeService::RuntimeStatus NativeHidRuntimeService::Inspect() const {
    RuntimeStatus status;
    status.packageExists = std::filesystem::exists(InfPath()) &&
        std::filesystem::exists(PackageDir() / L"XiaoJiVhid.sys") &&
        std::filesystem::exists(PackageDir() / L"XiaoJiVhid.cat");
    status.serviceInstalled = ServiceExists();
    status.sysInstalled = std::filesystem::exists(SystemDriverPath());
    const DeviceEnumerationResult devices = EnumerateDevices(false, nullptr);
    status.devicePresent = devices.present;
    status.deviceStarted = devices.started;
    if (!devices.publishedInfNames.empty()) {
        status.publishedInfName = *devices.publishedInfNames.begin();
    } else {
        status.publishedInfName = PublishedInfNameForPackage(InfPath());
    }
    status.interfacePresent = NativeHidDriver::DevicePresent();
    return status;
}

bool NativeHidRuntimeService::Install(std::wstring* resultMessage) const {
    std::wstringstream log;
    const auto infPath = std::filesystem::absolute(InfPath());
    if (!std::filesystem::exists(infPath) ||
        !std::filesystem::exists(PackageDir() / L"XiaoJiVhid.sys") ||
        !std::filesystem::exists(PackageDir() / L"XiaoJiVhid.cat")) {
        const std::wstring message = L"XiaoJi HID package is incomplete: " + PackageDir().wstring();
        if (resultMessage) *resultMessage = message;
        AppendLog(logsDir_, message);
        return false;
    }

    wchar_t publishedInf[MAX_PATH]{};
    DWORD required = 0;
    const bool staged = ::SetupCopyOEMInfW(infPath.c_str(), PackageDir().c_str(), SPOST_PATH,
        0, publishedInf, MAX_PATH, &required, nullptr) != FALSE || ::GetLastError() == ERROR_FILE_EXISTS;
    log << L"Stage package -> " << (staged ? L"ok" : FormatWin32Error(::GetLastError())) << L"\n";
    if (!staged) {
        if (resultMessage) *resultMessage = log.str();
        AppendLog(logsDir_, log.str());
        return false;
    }

    if (!EnumerateDevices(false, nullptr).present && !CreateRootDevice(&log)) {
        if (resultMessage) *resultMessage = log.str();
        AppendLog(logsDir_, log.str());
        return false;
    }

    BOOL rebootRequired = FALSE;
    const bool updated = ::UpdateDriverForPlugAndPlayDevicesW(nullptr,
        XIAOJI_VHID_HARDWARE_ID, infPath.c_str(), INSTALLFLAG_FORCE, &rebootRequired) != FALSE;
    log << L"Bind driver -> " << (updated ? L"ok" : FormatWin32Error(::GetLastError()))
        << L", reboot=" << (rebootRequired ? L"yes" : L"no") << L"\n";

    RuntimeStatus status;
    for (int attempt = 0; attempt < 30; ++attempt) {
        status = Inspect();
        if (status.serviceInstalled && status.sysInstalled && status.deviceStarted && status.interfacePresent) break;
        ::Sleep(100);
    }
    const bool ready = updated && status.serviceInstalled && status.sysInstalled &&
        status.deviceStarted && status.interfacePresent;
    log << L"Final state -> service=" << status.serviceInstalled
        << L", sys=" << status.sysInstalled
        << L", device=" << status.devicePresent
        << L", started=" << status.deviceStarted
        << L", interface=" << status.interfacePresent << L"\n";
    if (resultMessage) *resultMessage = log.str();
    AppendLog(logsDir_, log.str());
    return ready;
}

bool NativeHidRuntimeService::Uninstall(std::wstring* resultMessage) const {
    std::wstringstream log;
    DeviceEnumerationResult devices = EnumerateDevices(true, &log);
    const std::wstring packageInfName = PublishedInfNameForPackage(InfPath());
    if (!packageInfName.empty()) devices.publishedInfNames.insert(packageInfName);
    bool packagesRemoved = true;
    for (const std::wstring& infName : devices.publishedInfNames) {
        const bool removed = ::SetupUninstallOEMInfW(infName.c_str(), SUOI_FORCEDELETE, nullptr) != FALSE;
        packagesRemoved = packagesRemoved && removed;
        log << L"Remove package " << infName << L" -> "
            << (removed ? L"ok" : FormatWin32Error(::GetLastError())) << L"\n";
    }

    const RuntimeStatus status = Inspect();
    const bool removed = packagesRemoved && !status.devicePresent && !status.serviceInstalled;
    log << L"Final state -> service=" << status.serviceInstalled
        << L", sys=" << status.sysInstalled
        << L", device=" << status.devicePresent
        << L", interface=" << status.interfacePresent << L"\n";
    if (resultMessage) *resultMessage = log.str();
    AppendLog(logsDir_, log.str());
    return removed;
}

std::filesystem::path NativeHidRuntimeService::PackageDir() const {
    return exeDir_ / L"driver" / L"native";
}

std::filesystem::path NativeHidRuntimeService::InfPath() const {
    return PackageDir() / XIAOJI_VHID_INF_NAME;
}
