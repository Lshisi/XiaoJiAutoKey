#include "Ch552UsbDriver.h"

#include <SetupAPI.h>
#include <devguid.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cwctype>
#include <sstream>
#include <thread>
#include <vector>

namespace {
constexpr wchar_t kXiaoJiUsbVidPid[] = L"vid_1209&pid_584a";
constexpr char kUpdateCommand[] = "UPD 714021";
constexpr char kFastInputFirmwareVersion[] = "XiaoJi USB V1.1.3";

std::uint64_t EncodeKey(const KeyDescriptor& key) {
    return (static_cast<std::uint64_t>(key.device == InputDevice::Mouse ? 1 : 0) << 32) | key.code;
}

std::string BuildCommand(char op, const KeyDescriptor& key, bool fast = false) {
    std::ostringstream stream;
    if (fast) stream << 'F';
    stream << op << ' ' << (key.device == InputDevice::Mouse ? 1 : 0) << ' ' << key.code;
    return stream.str();
}

bool IsLatencySensitiveSource(InputSource src) {
    return src == InputSource::Repeater || src == InputSource::Macro;
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

std::vector<std::wstring> BuildAutoConnectPortList() {
    return EnumerateXiaoJiComPorts();
}
}  // namespace

Ch552UsbDriver::~Ch552UsbDriver() {
    Disconnect();
}

std::wstring Ch552UsbDriver::DetectPortName() {
    const auto ports = BuildAutoConnectPortList();
    return ports.empty() ? std::wstring{} : ports.front();
}

bool Ch552UsbDriver::AutoConnect() {
    std::lock_guard lock(mutex_);
    ClosePortLocked();
    lastError_.clear();

    std::wstring errors;
    const std::vector<std::wstring> ports = BuildAutoConnectPortList();
    if (ports.empty()) {
        lastError_ = L"No XiaoJi USB device found";
        return false;
    }

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
        ? L"No XiaoJi USB device found"
        : L"No XiaoJi USB device accepted handshake:\n" + errors;
    return false;
}

bool Ch552UsbDriver::Connect(const std::wstring& portName) {
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

void Ch552UsbDriver::Disconnect() {
    std::lock_guard lock(mutex_);
    if (handle_ != INVALID_HANDLE_VALUE) {
        SendCommandLocked("R");
    }
    ClosePortLocked();
}

bool Ch552UsbDriver::IsReady() const noexcept {
    std::lock_guard lock(mutex_);
    return handle_ != INVALID_HANDLE_VALUE;
}

std::wstring Ch552UsbDriver::GetLastError() const {
    std::lock_guard lock(mutex_);
    return lastError_;
}

std::wstring Ch552UsbDriver::PortName() const {
    std::lock_guard lock(mutex_);
    return portName_;
}

std::wstring Ch552UsbDriver::FirmwareVersion() const {
    std::lock_guard lock(mutex_);
    return firmwareVersion_;
}

bool Ch552UsbDriver::EnterBootloaderForUpdate() {
    std::lock_guard lock(mutex_);
    if (handle_ == INVALID_HANDLE_VALUE) return false;

    SendCommandLocked("R");

    DCB dcb{};
    dcb.DCBlength = sizeof(dcb);
    if (::GetCommState(handle_, &dcb)) {
        dcb.BaudRate = CBR_1200;
        dcb.fDtrControl = DTR_CONTROL_ENABLE;
        if (::SetCommState(handle_, &dcb)) {
            ::Sleep(20);
            // The device disconnects while Windows is processing CLRDTR, so
            // an EscapeCommFunction failure here can still mean success.
            ::EscapeCommFunction(handle_, CLRDTR);
            ::Sleep(30);
            ClosePortLocked();
            return true;
        }
    }

    const bool ok = SendCommandLocked(kUpdateCommand);
    if (ok) {
        ClosePortLocked();
    }
    return ok;
}

bool Ch552UsbDriver::SendTap(const KeyDescriptor& key, InputSource src) {
    if (!SendDown(key, src)) return false;
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    return SendUp(key, src);
}

bool Ch552UsbDriver::SendDown(const KeyDescriptor& key, InputSource src) {
    if (!key.IsValid()) return false;
    std::lock_guard lock(mutex_);
    if (handle_ == INVALID_HANDLE_VALUE) return false;
    const bool fast = fastInputSupported_ && IsLatencySensitiveSource(src);
    const bool sent = fast
        ? SendFastCommandLocked(BuildCommand('D', key, true))
        : SendCommandLocked(BuildCommand('D', key));
    if (!sent) return false;
    ++pressedKeys_[EncodeKey(key)];
    return true;
}

bool Ch552UsbDriver::SendUp(const KeyDescriptor& key, InputSource src) {
    if (!key.IsValid()) return false;
    std::lock_guard lock(mutex_);
    if (handle_ == INVALID_HANDLE_VALUE) return false;
    const bool fast = fastInputSupported_ && IsLatencySensitiveSource(src);
    const bool sent = fast
        ? SendFastCommandLocked(BuildCommand('U', key, true))
        : SendCommandLocked(BuildCommand('U', key));
    if (!sent) return false;
    const auto encoded = EncodeKey(key);
    auto it = pressedKeys_.find(encoded);
    if (it != pressedKeys_.end() && --it->second <= 0) {
        pressedKeys_.erase(it);
    }
    return true;
}

void Ch552UsbDriver::ReleaseAll() {
    std::lock_guard lock(mutex_);
    if (handle_ != INVALID_HANDLE_VALUE) {
        SendCommandLocked("R");
    }
    pressedKeys_.clear();
}

void Ch552UsbDriver::ClearAllStates() {
    std::lock_guard lock(mutex_);
    pressedKeys_.clear();
}

bool Ch552UsbDriver::OpenPortLocked(const std::wstring& portName) {
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
    // Poll reads without the old per-command inter-character timeout. The
    // caller owns the overall response deadline.
    timeouts.ReadIntervalTimeout = MAXDWORD;
    timeouts.ReadTotalTimeoutConstant = 0;
    timeouts.ReadTotalTimeoutMultiplier = 0;
    timeouts.WriteTotalTimeoutConstant = 80;
    timeouts.WriteTotalTimeoutMultiplier = 1;
    ::SetCommTimeouts(handle_, &timeouts);
    ::PurgeComm(handle_, PURGE_RXCLEAR | PURGE_TXCLEAR);
    portName_ = portName;
    return true;
}

void Ch552UsbDriver::ClosePortLocked() {
    if (handle_ != INVALID_HANDLE_VALUE) {
        ::CloseHandle(handle_);
        handle_ = INVALID_HANDLE_VALUE;
    }
    portName_.clear();
    firmwareVersion_.clear();
    fastResponseBuffer_.clear();
    fastInputSupported_ = false;
    pressedKeys_.clear();
}

bool Ch552UsbDriver::WriteLineLocked(const std::string& line) {
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

std::string Ch552UsbDriver::ReadAvailableLocked(DWORD waitMs) {
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
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
    return result;
}

void Ch552UsbDriver::DrainFastResponsesLocked() {
    std::array<char, 128> buffer{};
    while (handle_ != INVALID_HANDLE_VALUE) {
        DWORD bytesRead = 0;
        if (!::ReadFile(handle_, buffer.data(), static_cast<DWORD>(buffer.size()), &bytesRead, nullptr) ||
            bytesRead == 0) {
            break;
        }
        fastResponseBuffer_.append(buffer.data(), buffer.data() + bytesRead);
    }

    std::size_t newline = 0;
    while ((newline = fastResponseBuffer_.find('\n')) != std::string::npos) {
        std::string response = fastResponseBuffer_.substr(0, newline);
        fastResponseBuffer_.erase(0, newline + 1);
        response.erase(std::remove(response.begin(), response.end(), '\r'), response.end());
        if (response.rfind("ERR", 0) == 0) {
            lastError_ = L"USB fast input failed: " + ToWide(response);
        }
    }
}

bool Ch552UsbDriver::QueryVersionLocked() {
    firmwareVersion_.clear();
    ::PurgeComm(handle_, PURGE_RXCLEAR | PURGE_TXCLEAR);
    if (!WriteLineLocked("VER")) {
        return false;
    }
    const std::string response = ReadAvailableLocked(220);
    const bool currentProtocol = response.find("XiaoJi USB V") != std::string::npos;
    const bool legacyProtocol = response.find("XiaoJi USB Bridge V2.") != std::string::npos;
    if (response.rfind("OK ", 0) != 0 || (!currentProtocol && !legacyProtocol)) {
        lastError_ = L"Unexpected handshake response: " + ToWide(response);
        return false;
    }
    firmwareVersion_ = ToWide(response.substr(3));
    firmwareVersion_.erase(std::remove(firmwareVersion_.begin(), firmwareVersion_.end(), L'\r'), firmwareVersion_.end());
    firmwareVersion_.erase(std::remove(firmwareVersion_.begin(), firmwareVersion_.end(), L'\n'), firmwareVersion_.end());
    fastInputSupported_ = response.find(kFastInputFirmwareVersion) != std::string::npos;
    SendCommandLocked("R");
    return true;
}

bool Ch552UsbDriver::SendCommandLocked(const std::string& line) {
    DrainFastResponsesLocked();
    if (!WriteLineLocked(line)) {
        return false;
    }

    const DWORD started = ::GetTickCount();
    std::string rejectedResponse;
    while (::GetTickCount() - started < 120) {
        const DWORD elapsed = ::GetTickCount() - started;
        const std::string response = ReadAvailableLocked(std::max<DWORD>(1, 120 - elapsed));
        if (response.find("OK") != std::string::npos) {
            return true;
        }
        if (!response.empty()) {
            rejectedResponse += response;
        }
    }

    if (!rejectedResponse.empty()) {
        lastError_ = L"Device rejected command '" + ToWide(line) + L"': " + ToWide(rejectedResponse);
    } else {
        lastError_ = L"No response for command '" + ToWide(line) + L"'";
    }
    return false;
}

bool Ch552UsbDriver::SendFastCommandLocked(const std::string& line) {
    DrainFastResponsesLocked();
    return WriteLineLocked(line);
}

std::wstring Ch552UsbDriver::FormatWin32Error(DWORD errorCode) {
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

std::wstring Ch552UsbDriver::NormalizePortName(const std::wstring& portName) {
    if (portName.rfind(LR"(\\.\)", 0) == 0) {
        return portName;
    }
    return LR"(\\.\)" + portName;
}
