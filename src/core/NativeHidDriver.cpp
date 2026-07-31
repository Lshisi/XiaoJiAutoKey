#include <initguid.h>
#include "NativeHidDriver.h"
#include "XiaoJiVhidProtocol.h"

#include <SetupAPI.h>

#include <algorithm>
#include <chrono>
#include <thread>
#include <vector>

namespace {
constexpr ULONGLONG kInjectedEventTtlMs = 120;

template <typename T>
T MakeProtocolInput() {
    T value{};
    value.Header.Size = sizeof(T);
    value.Header.Version = XIAOJI_VHID_PROTOCOL_VERSION;
    return value;
}
}

NativeHidDriver::~NativeHidDriver() {
    Disconnect();
}

bool NativeHidDriver::Connect() {
    Disconnect();

    const std::wstring path = FindDevicePath();
    if (path.empty()) {
        std::lock_guard lock(mutex_);
        lastError_ = L"XiaoJi HID device interface was not found.";
        return false;
    }

    HANDLE handle = ::CreateFileW(
        path.c_str(),
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (handle == INVALID_HANDLE_VALUE) {
        std::lock_guard lock(mutex_);
        lastError_ = L"Unable to open XiaoJi HID device: " + FormatWin32Error(::GetLastError());
        return false;
    }

    {
        std::lock_guard lock(mutex_);
        handle_ = handle;
        XIAOJI_VHID_STATUS status{};
        if (!SendIoctlLocked(IOCTL_XIAOJI_VHID_GET_STATUS, nullptr, 0, &status, sizeof(status)) ||
            status.Size != sizeof(status) ||
            status.ProtocolVersion != XIAOJI_VHID_PROTOCOL_VERSION ||
            (status.Flags & XIAOJI_VHID_STATUS_STARTED) == 0) {
            lastError_ = L"XiaoJi HID protocol or device state is incompatible.";
            ::CloseHandle(handle_);
            handle_ = INVALID_HANDLE_VALUE;
            return false;
        }
        driverVersion_ = status.DriverVersion;
        if (!SendIoctlLocked(IOCTL_XIAOJI_VHID_ACQUIRE)) {
            const std::wstring error = lastError_;
            ::CloseHandle(handle_);
            handle_ = INVALID_HANDLE_VALUE;
            lastError_ = error.empty() ? L"XiaoJi HID device is already in use." : error;
            return false;
        }
        leaseOwned_ = true;
        pressedKeys_.clear();
        selfInjected_.clear();
        lastError_.clear();
    }

    heartbeatThread_ = std::jthread([this](std::stop_token token) { HeartbeatLoop(token); });
    return true;
}

void NativeHidDriver::Disconnect() {
    if (heartbeatThread_.joinable()) {
        heartbeatThread_.request_stop();
        heartbeatThread_.join();
    }

    std::lock_guard lock(mutex_);
    if (handle_ != INVALID_HANDLE_VALUE) {
        if (leaseOwned_) {
            (void)SendIoctlLocked(IOCTL_XIAOJI_VHID_RELEASE_ALL);
            (void)SendIoctlLocked(IOCTL_XIAOJI_VHID_RELEASE);
        }
        ::CloseHandle(handle_);
        handle_ = INVALID_HANDLE_VALUE;
    }
    leaseOwned_ = false;
    driverVersion_ = 0;
    pressedKeys_.clear();
    selfInjected_.clear();
}

bool NativeHidDriver::DevicePresent() {
    return !FindDevicePath().empty();
}

bool NativeHidDriver::IsReady() const noexcept {
    std::lock_guard lock(mutex_);
    return handle_ != INVALID_HANDLE_VALUE && leaseOwned_;
}

std::wstring NativeHidDriver::GetLastError() const {
    std::lock_guard lock(mutex_);
    return lastError_;
}

std::uint32_t NativeHidDriver::DriverVersion() const noexcept {
    std::lock_guard lock(mutex_);
    return driverVersion_;
}

bool NativeHidDriver::SendTap(const KeyDescriptor& key, InputSource src) {
    if (!SendDown(key, src)) return false;
    std::this_thread::sleep_for(std::chrono::milliseconds(kInputPulseWidthMs));
    return SendUp(key, src);
}

bool NativeHidDriver::SendDown(const KeyDescriptor& key, InputSource) {
    if (!key.IsValid()) return false;
    std::lock_guard lock(mutex_);
    if (handle_ == INVALID_HANDLE_VALUE || !leaseOwned_) return false;

    bool sent = false;
    if (key.device == InputDevice::Mouse) {
        const auto button = ToMouseButton(key.code);
        sent = button.has_value() && SendMouseButtonLocked(*button, true);
    } else if (const auto consumer = ToConsumerUsage(key.code); consumer.has_value()) {
        sent = SendConsumerLocked(*consumer, true);
    } else if (const auto usage = ToKeyboardUsage(key.code); usage.has_value()) {
        sent = SendKeyboardLocked(*usage, true);
    } else {
        lastError_ = L"Unsupported key for XiaoJi HID: " + std::to_wstring(key.code);
    }
    if (!sent) return false;

    ++pressedKeys_[EncodeKey(key)];
    MarkInjectedLocked(key, true);
    return true;
}

bool NativeHidDriver::SendUp(const KeyDescriptor& key, InputSource) {
    if (!key.IsValid()) return false;
    std::lock_guard lock(mutex_);
    if (handle_ == INVALID_HANDLE_VALUE || !leaseOwned_) return false;

    bool sent = false;
    if (key.device == InputDevice::Mouse) {
        const auto button = ToMouseButton(key.code);
        sent = button.has_value() && SendMouseButtonLocked(*button, false);
    } else if (const auto consumer = ToConsumerUsage(key.code); consumer.has_value()) {
        sent = SendConsumerLocked(*consumer, false);
    } else if (const auto usage = ToKeyboardUsage(key.code); usage.has_value()) {
        sent = SendKeyboardLocked(*usage, false);
    } else {
        lastError_ = L"Unsupported key for XiaoJi HID: " + std::to_wstring(key.code);
    }
    if (!sent) return false;

    const auto encoded = EncodeKey(key);
    auto it = pressedKeys_.find(encoded);
    if (it != pressedKeys_.end() && --it->second <= 0) {
        pressedKeys_.erase(it);
    }
    MarkInjectedLocked(key, false);
    return true;
}

void NativeHidDriver::ReleaseAll() {
    std::lock_guard lock(mutex_);
    if (handle_ != INVALID_HANDLE_VALUE && leaseOwned_) {
        (void)SendIoctlLocked(IOCTL_XIAOJI_VHID_RELEASE_ALL);
    }
    pressedKeys_.clear();
}

void NativeHidDriver::ClearAllStates() {
    std::lock_guard lock(mutex_);
    pressedKeys_.clear();
    selfInjected_.clear();
}

bool NativeHidDriver::ConsumeSelfInjected(const KeyDescriptor& key, bool pressed) {
    std::lock_guard lock(mutex_);
    auto it = selfInjected_.find(EncodeKey(key));
    if (it == selfInjected_.end()) return false;

    InjectedState& state = it->second;
    const ULONGLONG now = ::GetTickCount64();
    int& count = pressed ? state.downCount : state.upCount;
    const ULONGLONG tick = pressed ? state.lastDownTick : state.lastUpTick;
    if (count <= 0 || now - tick > kInjectedEventTtlMs) {
        if (now - std::max(state.lastDownTick, state.lastUpTick) > kInjectedEventTtlMs) {
            selfInjected_.erase(it);
        }
        return false;
    }
    --count;
    if (state.downCount == 0 && state.upCount == 0) {
        selfInjected_.erase(it);
    }
    return true;
}

std::wstring NativeHidDriver::FindDevicePath() {
    HDEVINFO deviceInfo = ::SetupDiGetClassDevsW(
        &GUID_DEVINTERFACE_XIAOJI_VHID, nullptr, nullptr,
        DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (deviceInfo == INVALID_HANDLE_VALUE) return {};

    std::wstring result;
    SP_DEVICE_INTERFACE_DATA interfaceData{};
    interfaceData.cbSize = sizeof(interfaceData);
    if (::SetupDiEnumDeviceInterfaces(deviceInfo, nullptr, &GUID_DEVINTERFACE_XIAOJI_VHID, 0, &interfaceData)) {
        DWORD required = 0;
        ::SetupDiGetDeviceInterfaceDetailW(deviceInfo, &interfaceData, nullptr, 0, &required, nullptr);
        if (required >= sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W)) {
            std::vector<BYTE> buffer(required);
            auto* detail = reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W*>(buffer.data());
            detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);
            if (::SetupDiGetDeviceInterfaceDetailW(
                    deviceInfo, &interfaceData, detail, required, nullptr, nullptr)) {
                result = detail->DevicePath;
            }
        }
    }
    ::SetupDiDestroyDeviceInfoList(deviceInfo);
    return result;
}

std::optional<std::uint16_t> NativeHidDriver::ToKeyboardUsage(std::uint32_t vk) {
    if (vk >= 'A' && vk <= 'Z') return static_cast<std::uint16_t>(0x04 + vk - 'A');
    if (vk >= '1' && vk <= '9') return static_cast<std::uint16_t>(0x1E + vk - '1');
    if (vk == '0') return 0x27;
    if (vk >= VK_F1 && vk <= VK_F12) return static_cast<std::uint16_t>(0x3A + vk - VK_F1);
    if (vk >= VK_F13 && vk <= VK_F24) return static_cast<std::uint16_t>(0x68 + vk - VK_F13);
    if (vk >= VK_NUMPAD1 && vk <= VK_NUMPAD9) return static_cast<std::uint16_t>(0x59 + vk - VK_NUMPAD1);

    switch (vk) {
    case VK_RETURN: return 0x28;
    case VK_ESCAPE: return 0x29;
    case VK_BACK: return 0x2A;
    case VK_TAB: return 0x2B;
    case VK_SPACE: return 0x2C;
    case VK_OEM_MINUS: return 0x2D;
    case VK_OEM_PLUS: return 0x2E;
    case VK_OEM_4: return 0x2F;
    case VK_OEM_6: return 0x30;
    case VK_OEM_5: return 0x31;
    case VK_OEM_1: return 0x33;
    case VK_OEM_7: return 0x34;
    case VK_OEM_3: return 0x35;
    case VK_OEM_COMMA: return 0x36;
    case VK_OEM_PERIOD: return 0x37;
    case VK_OEM_2: return 0x38;
    case VK_CAPITAL: return 0x39;
    case VK_SNAPSHOT: return 0x46;
    case VK_SCROLL: return 0x47;
    case VK_PAUSE: return 0x48;
    case VK_INSERT: return 0x49;
    case VK_HOME: return 0x4A;
    case VK_PRIOR: return 0x4B;
    case VK_DELETE: return 0x4C;
    case VK_END: return 0x4D;
    case VK_NEXT: return 0x4E;
    case VK_RIGHT: return 0x4F;
    case VK_LEFT: return 0x50;
    case VK_DOWN: return 0x51;
    case VK_UP: return 0x52;
    case VK_NUMLOCK: return 0x53;
    case VK_DIVIDE: return 0x54;
    case VK_MULTIPLY: return 0x55;
    case VK_SUBTRACT: return 0x56;
    case VK_ADD: return 0x57;
    case kNumpadEnterKeyCode: return 0x58;
    case VK_NUMPAD0: return 0x62;
    case VK_DECIMAL: return 0x63;
    case VK_OEM_102: return 0x64;
    case VK_APPS: return 0x65;
    case VK_CANCEL: return 0x66;
    case VK_CLEAR: return 0x9C;
    case VK_SHIFT:
    case VK_LSHIFT: return 0xE1;
    case VK_RSHIFT: return 0xE5;
    case VK_CONTROL:
    case VK_LCONTROL: return 0xE0;
    case VK_RCONTROL: return 0xE4;
    case VK_MENU:
    case VK_LMENU: return 0xE2;
    case VK_RMENU: return 0xE6;
    case VK_LWIN: return 0xE3;
    case VK_RWIN: return 0xE7;
    default: return std::nullopt;
    }
}

std::optional<std::uint16_t> NativeHidDriver::ToConsumerUsage(std::uint32_t vk) {
    switch (vk) {
    case VK_VOLUME_MUTE: return 0x00E2;
    case VK_VOLUME_UP: return 0x00E9;
    case VK_VOLUME_DOWN: return 0x00EA;
    case VK_MEDIA_NEXT_TRACK: return 0x00B5;
    case VK_MEDIA_PREV_TRACK: return 0x00B6;
    case VK_MEDIA_STOP: return 0x00B7;
    case VK_MEDIA_PLAY_PAUSE: return 0x00CD;
    case VK_BROWSER_SEARCH: return 0x0221;
    case VK_BROWSER_HOME: return 0x0223;
    case VK_BROWSER_BACK: return 0x0224;
    case VK_BROWSER_FORWARD: return 0x0225;
    case VK_BROWSER_STOP: return 0x0226;
    case VK_BROWSER_REFRESH: return 0x0227;
    case VK_BROWSER_FAVORITES: return 0x022A;
    case VK_LAUNCH_MAIL: return 0x018A;
    case VK_LAUNCH_MEDIA_SELECT: return 0x0183;
    case VK_LAUNCH_APP1: return 0x0194;
    case VK_LAUNCH_APP2: return 0x0192;
    default: return std::nullopt;
    }
}

std::optional<std::uint8_t> NativeHidDriver::ToMouseButton(std::uint32_t vk) {
    switch (vk) {
    case VK_LBUTTON: return 1;
    case VK_RBUTTON: return 2;
    case VK_MBUTTON: return 3;
    case VK_XBUTTON1: return 4;
    case VK_XBUTTON2: return 5;
    default: return std::nullopt;
    }
}

std::uint64_t NativeHidDriver::EncodeKey(const KeyDescriptor& key) noexcept {
    return (static_cast<std::uint64_t>(key.device == InputDevice::Mouse ? 1 : 0) << 32) | key.code;
}

std::wstring NativeHidDriver::FormatWin32Error(DWORD errorCode) {
    wchar_t* buffer = nullptr;
    const DWORD length = ::FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, errorCode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<LPWSTR>(&buffer), 0, nullptr);
    std::wstring result = std::to_wstring(errorCode);
    if (length > 0 && buffer) result += L" (" + std::wstring(buffer) + L")";
    if (buffer) ::LocalFree(buffer);
    return result;
}

bool NativeHidDriver::SendKeyboardLocked(std::uint16_t usage, bool pressed) {
    auto input = MakeProtocolInput<XIAOJI_VHID_KEYBOARD_INPUT>();
    input.Usage = usage;
    input.Pressed = pressed ? 1 : 0;
    return SendIoctlLocked(IOCTL_XIAOJI_VHID_KEYBOARD, &input, sizeof(input));
}

bool NativeHidDriver::SendConsumerLocked(std::uint16_t usage, bool pressed) {
    auto input = MakeProtocolInput<XIAOJI_VHID_CONSUMER_INPUT>();
    input.Usage = usage;
    input.Pressed = pressed ? 1 : 0;
    return SendIoctlLocked(IOCTL_XIAOJI_VHID_CONSUMER, &input, sizeof(input));
}

bool NativeHidDriver::SendMouseButtonLocked(std::uint8_t button, bool pressed) {
    auto input = MakeProtocolInput<XIAOJI_VHID_MOUSE_BUTTON_INPUT>();
    input.Button = button;
    input.Pressed = pressed ? 1 : 0;
    return SendIoctlLocked(IOCTL_XIAOJI_VHID_MOUSE_BUTTON, &input, sizeof(input));
}

bool NativeHidDriver::SendIoctlLocked(DWORD ioctl, const void* input, DWORD inputSize,
                                      void* output, DWORD outputSize) {
    if (handle_ == INVALID_HANDLE_VALUE) return false;
    DWORD returned = 0;
    if (::DeviceIoControl(handle_, ioctl, const_cast<void*>(input), inputSize,
            output, outputSize, &returned, nullptr)) {
        return true;
    }
    lastError_ = L"XiaoJi HID IOCTL failed: " + FormatWin32Error(::GetLastError());
    return false;
}

void NativeHidDriver::HeartbeatLoop(std::stop_token stopToken) {
    while (!stopToken.stop_requested()) {
        for (int i = 0; i < 10 && !stopToken.stop_requested(); ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        if (stopToken.stop_requested()) break;

        std::lock_guard lock(mutex_);
        if (handle_ == INVALID_HANDLE_VALUE) continue;
        if (!SendIoctlLocked(IOCTL_XIAOJI_VHID_HEARTBEAT)) {
            // The kernel watchdog revokes stale owners. Reacquire when scheduling resumes.
            leaseOwned_ = false;
            if (SendIoctlLocked(IOCTL_XIAOJI_VHID_ACQUIRE)) {
                leaseOwned_ = true;
                pressedKeys_.clear();
                selfInjected_.clear();
                lastError_.clear();
            }
        }
    }
}

void NativeHidDriver::MarkInjectedLocked(const KeyDescriptor& key, bool pressed) {
    InjectedState& state = selfInjected_[EncodeKey(key)];
    if (pressed) {
        ++state.downCount;
        state.lastDownTick = ::GetTickCount64();
    } else {
        ++state.upCount;
        state.lastUpTick = ::GetTickCount64();
    }
}
