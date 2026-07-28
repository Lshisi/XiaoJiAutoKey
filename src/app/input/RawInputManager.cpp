#include "input/RawInputManager.h"

#include <SetupAPI.h>

#include <algorithm>
#include <cwctype>
#include <vector>

namespace {

std::wstring ToLower(std::wstring value) {
    std::transform(value.begin(), value.end(), value.begin(), [](wchar_t ch) {
        return static_cast<wchar_t>(std::towlower(ch));
    });
    return value;
}

bool ContainsAnyVirtualToken(const std::wstring& lowerName) {
    constexpr const wchar_t* tokens[] = {
        L"ddhid",
        L"ddhid63340",
        L"dd.key",
        L"dd.mou",
        L"ddxoft",
        L"root#ddhid",
        L"vid_1209&pid_584a",
    };
    for (const auto* token : tokens) {
        if (lowerName.find(token) != std::wstring::npos) {
            return true;
        }
    }
    return false;
}

std::wstring QueryRegistryProperty(
    HDEVINFO deviceInfo,
    SP_DEVINFO_DATA& deviceData,
    DWORD property) {
    DWORD propertyType = 0;
    DWORD requiredSize = 0;
    ::SetupDiGetDeviceRegistryPropertyW(
        deviceInfo,
        &deviceData,
        property,
        &propertyType,
        nullptr,
        0,
        &requiredSize);
    if (requiredSize == 0) {
        return {};
    }

    std::vector<BYTE> buffer(requiredSize + sizeof(wchar_t), 0);
    if (!::SetupDiGetDeviceRegistryPropertyW(
            deviceInfo,
            &deviceData,
            property,
            &propertyType,
            buffer.data(),
            static_cast<DWORD>(buffer.size()),
            nullptr)) {
        return {};
    }

    std::wstring value(
        reinterpret_cast<const wchar_t*>(buffer.data()),
        buffer.size() / sizeof(wchar_t));
    std::replace(value.begin(), value.end(), L'\0', L' ');
    return value;
}

std::wstring QueryDeviceIdentity(const std::wstring& devicePath) {
    HDEVINFO deviceInfo = ::SetupDiCreateDeviceInfoList(nullptr, nullptr);
    if (deviceInfo == INVALID_HANDLE_VALUE) {
        return {};
    }

    SP_DEVICE_INTERFACE_DATA interfaceData{};
    interfaceData.cbSize = sizeof(interfaceData);
    if (!::SetupDiOpenDeviceInterfaceW(deviceInfo, devicePath.c_str(), 0, &interfaceData)) {
        ::SetupDiDestroyDeviceInfoList(deviceInfo);
        return {};
    }

    DWORD requiredSize = 0;
    ::SetupDiGetDeviceInterfaceDetailW(
        deviceInfo,
        &interfaceData,
        nullptr,
        0,
        &requiredSize,
        nullptr);
    if (requiredSize == 0) {
        ::SetupDiDestroyDeviceInfoList(deviceInfo);
        return {};
    }

    std::vector<BYTE> detailBuffer(requiredSize);
    auto* detailData = reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W*>(detailBuffer.data());
    detailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);
    SP_DEVINFO_DATA deviceData{};
    deviceData.cbSize = sizeof(deviceData);
    if (!::SetupDiGetDeviceInterfaceDetailW(
            deviceInfo,
            &interfaceData,
            detailData,
            requiredSize,
            nullptr,
            &deviceData)) {
        ::SetupDiDestroyDeviceInfoList(deviceInfo);
        return {};
    }

    std::wstring identity;
    constexpr DWORD properties[] = {
        SPDRP_HARDWAREID,
        SPDRP_COMPATIBLEIDS,
        SPDRP_SERVICE,
        SPDRP_DEVICEDESC,
        SPDRP_FRIENDLYNAME,
    };
    for (const DWORD property : properties) {
        identity += L' ';
        identity += QueryRegistryProperty(deviceInfo, deviceData, property);
    }

    ::SetupDiDestroyDeviceInfoList(deviceInfo);
    return identity;
}

}  // namespace

RawInputManager::RawInputManager(QObject* parent) : QObject(parent) {}

RawInputManager::~RawInputManager() {
    unregisterWindow();
}

bool RawInputManager::registerWindow(HWND hwnd) {
    if (!hwnd) {
        keyboardActive_ = false;
        mouseActive_ = false;
        hwnd_ = nullptr;
        return false;
    }

    const auto registerDevice = [hwnd](USHORT usage) {
        RAWINPUTDEVICE device{};
        device.usUsagePage = 0x01;
        device.usUsage = usage;
        device.dwFlags = RIDEV_INPUTSINK;
        device.hwndTarget = hwnd;
        return RegisterRawInputDevices(&device, 1, sizeof(device)) != FALSE;
    };

    keyboardActive_ = registerDevice(0x06); // Keyboard
    mouseActive_ = registerDevice(0x02);    // Mouse
    hwnd_ = hwnd;
    deviceCache_.clear();
    return keyboardActive_ && mouseActive_;
}

void RawInputManager::unregisterWindow() {
    if (!keyboardActive_ && !mouseActive_) {
        hwnd_ = nullptr;
        return;
    }

    const auto removeDevice = [](USHORT usage) {
        RAWINPUTDEVICE device{};
        device.usUsagePage = 0x01;
        device.usUsage = usage;
        device.dwFlags = RIDEV_REMOVE;
        device.hwndTarget = nullptr;
        RegisterRawInputDevices(&device, 1, sizeof(device));
    };

    if (keyboardActive_) removeDevice(0x06);
    if (mouseActive_) removeDevice(0x02);

    hwnd_ = nullptr;
    keyboardActive_ = false;
    mouseActive_ = false;
    deviceCache_.clear();
}

bool RawInputManager::isActive() const noexcept {
    return keyboardActive_;
}

bool RawInputManager::isMouseActive() const noexcept {
    return mouseActive_;
}

bool RawInputManager::handleNativeMessage(MSG* message, long* result) {
    if ((!keyboardActive_ && !mouseActive_) || !message || message->message != WM_INPUT) {
        return false;
    }

    RAWINPUT raw{};
    UINT requiredSize = sizeof(raw);
    const auto rawInputHandle = reinterpret_cast<HRAWINPUT>(message->lParam);
    const UINT copied = GetRawInputData(
        rawInputHandle,
        RID_INPUT,
        &raw,
        &requiredSize,
        sizeof(RAWINPUTHEADER));
    if (copied == static_cast<UINT>(-1) || copied < sizeof(RAWINPUTHEADER) || copied > sizeof(raw)) {
        return false;
    }

    if (!shouldAcceptDevice(raw.header.hDevice)) {
        if (result) {
            *result = 0;
        }
        return false;
    }

    if (raw.header.dwType == RIM_TYPEKEYBOARD && keyboardActive_) {
        const RAWKEYBOARD& keyboard = raw.data.keyboard;
        const bool pressed = keyboard.Message == WM_KEYDOWN || keyboard.Message == WM_SYSKEYDOWN;
        const bool released = keyboard.Message == WM_KEYUP || keyboard.Message == WM_SYSKEYUP ||
            (keyboard.Flags & RI_KEY_BREAK) != 0;
        if (!pressed && !released) {
            return false;
        }

        const KeyDescriptor key = rawKeyboardKeyDescriptor(keyboard);
        if (!key.IsValid()) {
            return false;
        }
        emit keyboardChanged(key, pressed);
    } else if (raw.header.dwType == RIM_TYPEMOUSE && mouseActive_) {
        emitRawMouseChanges(raw.data.mouse);
    } else {
        return false;
    }

    if (result) {
        *result = 0;
    }
    return false;
}

std::optional<RawInputManager::DeviceInfo> RawInputManager::queryDeviceInfo(HANDLE device) {
    if (!device) {
        return DeviceInfo{};
    }

    UINT requiredChars = 0;
    if (GetRawInputDeviceInfoW(device, RIDI_DEVICENAME, nullptr, &requiredChars) != 0 || requiredChars == 0) {
        return std::nullopt;
    }

    std::wstring name(requiredChars, L'\0');
    if (GetRawInputDeviceInfoW(device, RIDI_DEVICENAME, name.data(), &requiredChars) == static_cast<UINT>(-1)) {
        return std::nullopt;
    }
    while (!name.empty() && name.back() == L'\0') {
        name.pop_back();
    }

    const std::wstring lowerIdentity = ToLower(name + QueryDeviceIdentity(name));
    DeviceInfo info;
    info.name = std::move(name);
    info.virtualInjector = ContainsAnyVirtualToken(lowerIdentity);
    return info;
}

bool RawInputManager::shouldAcceptDevice(HANDLE device) {
    const auto it = deviceCache_.find(device);
    if (it != deviceCache_.end()) {
        return !it->second.virtualInjector;
    }

    auto info = queryDeviceInfo(device);
    if (!info.has_value()) {
        // Unknown devices are treated as physical input so Raw Input cannot accidentally drop a real keyboard.
        return true;
    }

    const bool accepted = !info->virtualInjector;
    deviceCache_.emplace(device, std::move(*info));
    return accepted;
}

KeyDescriptor RawInputManager::rawKeyboardKeyDescriptor(const RAWKEYBOARD& keyboard) const {
    if (keyboard.VKey == 0 || keyboard.VKey == 0xFF || keyboard.VKey == VK_PACKET) {
        return {};
    }

    std::uint32_t flags = 0;
    if ((keyboard.Flags & RI_KEY_E0) != 0) {
        flags |= LLKHF_EXTENDED;
    }

    return KeyboardHookKeyDescriptor(
        static_cast<std::uint32_t>(keyboard.VKey),
        static_cast<std::uint32_t>(keyboard.MakeCode),
        flags);
}

void RawInputManager::emitRawMouseChanges(const RAWMOUSE& mouse) {
    const USHORT flags = mouse.usButtonFlags;
    const auto emitButton = [this, flags](USHORT downFlag, USHORT upFlag, std::uint32_t vk) {
        const KeyDescriptor key{InputDevice::Mouse, vk};
        if ((flags & downFlag) != 0) emit mouseChanged(key, true);
        if ((flags & upFlag) != 0) emit mouseChanged(key, false);
    };

    emitButton(RI_MOUSE_LEFT_BUTTON_DOWN, RI_MOUSE_LEFT_BUTTON_UP, VK_LBUTTON);
    emitButton(RI_MOUSE_RIGHT_BUTTON_DOWN, RI_MOUSE_RIGHT_BUTTON_UP, VK_RBUTTON);
    emitButton(RI_MOUSE_MIDDLE_BUTTON_DOWN, RI_MOUSE_MIDDLE_BUTTON_UP, VK_MBUTTON);
    emitButton(RI_MOUSE_BUTTON_4_DOWN, RI_MOUSE_BUTTON_4_UP, VK_XBUTTON1);
    emitButton(RI_MOUSE_BUTTON_5_DOWN, RI_MOUSE_BUTTON_5_UP, VK_XBUTTON2);
}
