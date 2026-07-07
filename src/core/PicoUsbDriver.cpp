#include "PicoUsbDriver.h"

#include <SetupAPI.h>
#include <devguid.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cwctype>
#include <set>
#include <sstream>
#include <thread>
#include <vector>

namespace {
constexpr wchar_t kXiaoJiUsbVidPid[] = L"vid_1209&pid_584a";

std::uint64_t EncodeKey(const KeyDescriptor& key) {
    return (static_cast<std::uint64_t>(key.device == InputDevice::Mouse ? 1 : 0) << 32) | key.code;
}

std::string BuildCommand(char op, const KeyDescriptor& key) {
    std::ostringstream stream;
    stream << op << ' ' << (key.device == InputDevice::Mouse ? 1 : 0) << ' ' << key.code;
    return stream.str();
}

std::wstring ToLower(std::wstring value) {
    std::transform(value.begin(), value.end(), value.begin(), [](wchar_t ch) {
        return static_cast<wchar_t>(std::towlower(ch));
    });
    return value;
}

int ComNumber(const std::wstring& portName) {
    if (portName.size() <= 3 ||
        (portName[0] != L'C' && portName[0] != L'c') ||
        (portName[1] != L'O' && portName[1] != L'o') ||
        (portName[2] != L'M' && portName[2] != L'm')) {
        return 0;
    }
    try {
        return std::stoi(portName.substr(3));
    } catch (...) {
        return 0;
    }
}

void AddUniquePort(std::vector<std::wstring>& ports, const std::wstring& portName) {
    if (portName.empty()) {
        return;
    }
    const auto exists = std::find_if(ports.begin(), ports.end(), [&](const std::wstring& existing) {
        return _wcsicmp(existing.c_str(), portName.c_str()) == 0;
    }) != ports.end();
    if (!exists) {
        ports.push_back(portName);
    }
}

std::wstring ExtractComPortName(const std::wstring& text) {
    const std::size_t close = text.rfind(L')');
    if (close == std::wstring::npos) {
        return {};
    }
    const std::size_t open = text.rfind(L'(', close);
    if (open == std::wstring::npos || close <= open + 1) {
        return {};
    }
    const std::wstring candidate = text.substr(open + 1, close - open - 1);
    return ComNumber(candidate) > 0 ? candidate : std::wstring{};
}

bool MultiSzContainsXiaoJiVidPid(const wchar_t* multiSz, DWORD byteCount) {
    if (multiSz == nullptr || byteCount < sizeof(wchar_t)) {
        return false;
    }
    const auto wcharCount = byteCount / sizeof(wchar_t);
    std::wstring combined;
    combined.reserve(wcharCount);
    for (std::size_t i = 0; i < wcharCount; ++i) {
        const wchar_t ch = multiSz[i];
        combined.push_back(ch == L'\0' ? L' ' : ch);
    }
    return ToLower(std::move(combined)).find(kXiaoJiUsbVidPid) != std::wstring::npos;
}

std::optional<std::wstring> QueryDeviceStringProperty(
    HDEVINFO deviceInfo,
    SP_DEVINFO_DATA& deviceData,
    DWORD property) {
    DWORD requiredSize = 0;
    DWORD propertyType = 0;
    ::SetupDiGetDeviceRegistryPropertyW(
        deviceInfo,
        &deviceData,
        property,
        &propertyType,
        nullptr,
        0,
        &requiredSize);
    if (requiredSize == 0) {
        return std::nullopt;
    }

    std::vector<BYTE> buffer(requiredSize + sizeof(wchar_t));
    if (!::SetupDiGetDeviceRegistryPropertyW(
            deviceInfo,
            &deviceData,
            property,
            &propertyType,
            buffer.data(),
            static_cast<DWORD>(buffer.size()),
            nullptr)) {
        return std::nullopt;
    }

    return std::wstring(reinterpret_cast<const wchar_t*>(buffer.data()));
}

std::vector<std::wstring> EnumerateXiaoJiComPorts() {
    std::vector<std::wstring> ports;
    HDEVINFO deviceInfo = ::SetupDiGetClassDevsW(
        &GUID_DEVCLASS_PORTS,
        nullptr,
        nullptr,
        DIGCF_PRESENT);
    if (deviceInfo == INVALID_HANDLE_VALUE) {
        return ports;
    }

    for (DWORD index = 0;; ++index) {
        SP_DEVINFO_DATA deviceData{};
        deviceData.cbSize = sizeof(deviceData);
        if (!::SetupDiEnumDeviceInfo(deviceInfo, index, &deviceData)) {
            break;
        }

        DWORD hardwareIdSize = 0;
        DWORD propertyType = 0;
        ::SetupDiGetDeviceRegistryPropertyW(
            deviceInfo,
            &deviceData,
            SPDRP_HARDWAREID,
            &propertyType,
            nullptr,
            0,
            &hardwareIdSize);
        std::vector<BYTE> hardwareIds(hardwareIdSize + sizeof(wchar_t));
        const bool hasHardwareIds = hardwareIdSize > 0 &&
            ::SetupDiGetDeviceRegistryPropertyW(
                deviceInfo,
                &deviceData,
                SPDRP_HARDWAREID,
                &propertyType,
                hardwareIds.data(),
                static_cast<DWORD>(hardwareIds.size()),
                nullptr);
        if (!hasHardwareIds ||
            !MultiSzContainsXiaoJiVidPid(reinterpret_cast<const wchar_t*>(hardwareIds.data()), hardwareIdSize)) {
            continue;
        }

        if (const auto friendlyName = QueryDeviceStringProperty(deviceInfo, deviceData, SPDRP_FRIENDLYNAME);
            friendlyName.has_value()) {
            AddUniquePort(ports, ExtractComPortName(*friendlyName));
        }
    }

    ::SetupDiDestroyDeviceInfoList(deviceInfo);
    std::sort(ports.begin(), ports.end(), [](const auto& a, const auto& b) {
        return ComNumber(a) > ComNumber(b);
    });
    return ports;
}

std::vector<std::wstring> EnumerateRegistryComPorts() {
    std::vector<std::wstring> ports;
    HKEY key = nullptr;
    if (::RegOpenKeyExW(
            HKEY_LOCAL_MACHINE,
            LR"(HARDWARE\DEVICEMAP\SERIALCOMM)",
            0,
            KEY_READ,
            &key) != ERROR_SUCCESS) {
        return ports;
    }

    for (DWORD index = 0;; ++index) {
        wchar_t valueName[256]{};
        DWORD valueNameChars = std::size(valueName);
        DWORD type = 0;
        wchar_t data[256]{};
        DWORD dataBytes = sizeof(data);
        const LONG result = ::RegEnumValueW(
            key,
            index,
            valueName,
            &valueNameChars,
            nullptr,
            &type,
            reinterpret_cast<LPBYTE>(data),
            &dataBytes);
        if (result == ERROR_NO_MORE_ITEMS) {
            break;
        }
        if (result == ERROR_SUCCESS && type == REG_SZ) {
            AddUniquePort(ports, data);
        }
    }
    ::RegCloseKey(key);

    std::sort(ports.begin(), ports.end(), [](const auto& a, const auto& b) {
        return ComNumber(a) > ComNumber(b);
    });
    return ports;
}

std::vector<std::wstring> BuildAutoConnectPortList() {
    std::vector<std::wstring> ports = EnumerateXiaoJiComPorts();
    for (const auto& port : EnumerateRegistryComPorts()) {
        AddUniquePort(ports, port);
    }
    if (ports.empty()) {
        for (int i = 16; i >= 1; --i) {
            ports.push_back(L"COM" + std::to_wstring(i));
        }
    }
    return ports;
}
}  // namespace

PicoUsbDriver::~PicoUsbDriver() {
    Disconnect();
}

bool PicoUsbDriver::AutoConnect() {
    std::lock_guard lock(mutex_);
    ClosePortLocked();
    lastError_.clear();

    std::wstring errors;
    const std::vector<std::wstring> ports = BuildAutoConnectPortList();
    for (const auto& port : ports) {
        if (!OpenPortLocked(port)) {
            continue;
        }
        if (QueryVersionLocked()) {
            return true;
        }
        if (!lastError_.empty()) {
            errors += port + L": " + lastError_ + L"\n";
        }
        ClosePortLocked();
    }

    lastError_ = errors.empty()
        ? L"No XiaoJi Pico USB serial device found"
        : L"No XiaoJi Pico USB serial device accepted handshake:\n" + errors;
    return false;
}

bool PicoUsbDriver::Connect(const std::wstring& portName) {
    std::lock_guard lock(mutex_);
    ClosePortLocked();
    if (!OpenPortLocked(portName)) {
        return false;
    }
    if (!QueryVersionLocked()) {
        ClosePortLocked();
        return false;
    }
    return true;
}

void PicoUsbDriver::Disconnect() {
    std::lock_guard lock(mutex_);
    if (handle_ != INVALID_HANDLE_VALUE) {
        SendCommandLocked("R");
    }
    ClosePortLocked();
}

bool PicoUsbDriver::IsReady() const noexcept {
    std::lock_guard lock(mutex_);
    return handle_ != INVALID_HANDLE_VALUE;
}

std::wstring PicoUsbDriver::GetLastError() const {
    std::lock_guard lock(mutex_);
    return lastError_;
}

std::wstring PicoUsbDriver::PortName() const {
    std::lock_guard lock(mutex_);
    return portName_;
}

std::wstring PicoUsbDriver::FirmwareVersion() const {
    std::lock_guard lock(mutex_);
    return firmwareVersion_;
}

bool PicoUsbDriver::SendTap(const KeyDescriptor& key, InputSource src) {
    if (!SendDown(key, src)) return false;
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    return SendUp(key, src);
}

bool PicoUsbDriver::SendDown(const KeyDescriptor& key, InputSource) {
    if (!key.IsValid()) return false;
    std::lock_guard lock(mutex_);
    if (handle_ == INVALID_HANDLE_VALUE) return false;
    if (!SendCommandLocked(BuildCommand('D', key))) return false;
    ++pressedKeys_[EncodeKey(key)];
    return true;
}

bool PicoUsbDriver::SendUp(const KeyDescriptor& key, InputSource) {
    if (!key.IsValid()) return false;
    std::lock_guard lock(mutex_);
    if (handle_ == INVALID_HANDLE_VALUE) return false;
    if (!SendCommandLocked(BuildCommand('U', key))) return false;
    const auto encoded = EncodeKey(key);
    auto it = pressedKeys_.find(encoded);
    if (it != pressedKeys_.end() && --it->second <= 0) {
        pressedKeys_.erase(it);
    }
    return true;
}

void PicoUsbDriver::ReleaseAll() {
    std::lock_guard lock(mutex_);
    if (handle_ != INVALID_HANDLE_VALUE) {
        SendCommandLocked("R");
    }
    pressedKeys_.clear();
}

void PicoUsbDriver::ClearAllStates() {
    std::lock_guard lock(mutex_);
    pressedKeys_.clear();
}

bool PicoUsbDriver::OpenPortLocked(const std::wstring& portName) {
    const std::wstring normalized = NormalizePortName(portName);
    handle_ = ::CreateFileW(
        normalized.c_str(),
        GENERIC_READ | GENERIC_WRITE,
        0,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (handle_ == INVALID_HANDLE_VALUE) {
        return false;
    }

    DCB dcb{};
    dcb.DCBlength = sizeof(dcb);
    if (!::GetCommState(handle_, &dcb)) {
        lastError_ = L"GetCommState failed: " + FormatWin32Error(::GetLastError());
        ClosePortLocked();
        return false;
    }
    dcb.BaudRate = CBR_115200;
    dcb.ByteSize = 8;
    dcb.Parity = NOPARITY;
    dcb.StopBits = ONESTOPBIT;
    dcb.fDtrControl = DTR_CONTROL_ENABLE;
    dcb.fRtsControl = RTS_CONTROL_ENABLE;
    if (!::SetCommState(handle_, &dcb)) {
        lastError_ = L"SetCommState failed: " + FormatWin32Error(::GetLastError());
        ClosePortLocked();
        return false;
    }

    COMMTIMEOUTS timeouts{};
    timeouts.ReadIntervalTimeout = 10;
    timeouts.ReadTotalTimeoutConstant = 60;
    timeouts.ReadTotalTimeoutMultiplier = 1;
    timeouts.WriteTotalTimeoutConstant = 80;
    timeouts.WriteTotalTimeoutMultiplier = 1;
    ::SetCommTimeouts(handle_, &timeouts);
    ::PurgeComm(handle_, PURGE_RXCLEAR | PURGE_TXCLEAR);
    portName_ = portName;
    return true;
}

void PicoUsbDriver::ClosePortLocked() {
    if (handle_ != INVALID_HANDLE_VALUE) {
        ::CloseHandle(handle_);
        handle_ = INVALID_HANDLE_VALUE;
    }
    portName_.clear();
    firmwareVersion_.clear();
    pressedKeys_.clear();
}

bool PicoUsbDriver::WriteLineLocked(const std::string& line) {
    if (handle_ == INVALID_HANDLE_VALUE) {
        lastError_ = L"Serial port is not open";
        return false;
    }
    const std::string wire = line + "\n";
    DWORD written = 0;
    if (!::WriteFile(handle_, wire.data(), static_cast<DWORD>(wire.size()), &written, nullptr) ||
        written != wire.size()) {
        lastError_ = L"WriteFile failed: " + FormatWin32Error(::GetLastError());
        return false;
    }
    return true;
}

std::string PicoUsbDriver::ReadAvailableLocked(DWORD waitMs) {
    std::string result;
    const DWORD started = ::GetTickCount();
    std::array<char, 128> buffer{};
    while (::GetTickCount() - started < waitMs) {
        DWORD bytesRead = 0;
        if (::ReadFile(handle_, buffer.data(), static_cast<DWORD>(buffer.size()), &bytesRead, nullptr) && bytesRead > 0) {
            result.append(buffer.data(), buffer.data() + bytesRead);
            if (result.find('\n') != std::string::npos) {
                break;
            }
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(8));
        }
    }
    return result;
}

bool PicoUsbDriver::QueryVersionLocked() {
    firmwareVersion_.clear();
    ::PurgeComm(handle_, PURGE_RXCLEAR | PURGE_TXCLEAR);
    if (!WriteLineLocked("VER")) {
        return false;
    }
    const std::string response = ReadAvailableLocked(220);
    if (response.find("OK xiaoji-pico-cpp") == std::string::npos) {
        lastError_ = L"Unexpected handshake response: " + ToWide(response);
        return false;
    }
    firmwareVersion_ = ToWide(response);
    firmwareVersion_.erase(std::remove(firmwareVersion_.begin(), firmwareVersion_.end(), L'\r'), firmwareVersion_.end());
    firmwareVersion_.erase(std::remove(firmwareVersion_.begin(), firmwareVersion_.end(), L'\n'), firmwareVersion_.end());
    SendCommandLocked("R");
    return true;
}

bool PicoUsbDriver::SendCommandLocked(const std::string& line) {
    if (!WriteLineLocked(line)) {
        return false;
    }
    const std::string response = ReadAvailableLocked(120);
    if (response.find("OK") != std::string::npos) {
        return true;
    }
    if (!response.empty()) {
        lastError_ = L"Device rejected command '" + ToWide(line) + L"': " + ToWide(response);
    } else {
        lastError_ = L"No response for command '" + ToWide(line) + L"'";
    }
    return false;
}

std::wstring PicoUsbDriver::FormatWin32Error(DWORD errorCode) {
    if (errorCode == ERROR_SUCCESS) return L"0";
    wchar_t* buffer = nullptr;
    const DWORD length = ::FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        errorCode,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<LPWSTR>(&buffer),
        0,
        nullptr);
    std::wstring result = std::to_wstring(errorCode);
    if (length > 0 && buffer) {
        result += L" (" + std::wstring(buffer) + L")";
    }
    if (buffer) ::LocalFree(buffer);
    return result;
}

std::wstring PicoUsbDriver::NormalizePortName(const std::wstring& portName) {
    if (portName.rfind(LR"(\\.\)", 0) == 0) {
        return portName;
    }
    return LR"(\\.\)" + portName;
}
