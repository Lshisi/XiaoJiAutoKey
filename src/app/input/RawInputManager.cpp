#include "input/RawInputManager.h"

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
        L"ddxoft",
        L"root#ddhid",
    };
    for (const auto* token : tokens) {
        if (lowerName.find(token) != std::wstring::npos) {
            return true;
        }
    }
    return false;
}

}  // namespace

RawInputManager::RawInputManager(QObject* parent) : QObject(parent) {}

RawInputManager::~RawInputManager() {
    unregisterWindow();
}

bool RawInputManager::registerWindow(HWND hwnd) {
    if (!hwnd) {
        active_ = false;
        hwnd_ = nullptr;
        return false;
    }

    RAWINPUTDEVICE device{};
    device.usUsagePage = 0x01;
    device.usUsage = 0x06; // Keyboard
    device.dwFlags = RIDEV_INPUTSINK;
    device.hwndTarget = hwnd;

    if (!RegisterRawInputDevices(&device, 1, sizeof(device))) {
        active_ = false;
        hwnd_ = nullptr;
        return false;
    }

    hwnd_ = hwnd;
    active_ = true;
    deviceCache_.clear();
    return true;
}

void RawInputManager::unregisterWindow() {
    if (!active_) {
        hwnd_ = nullptr;
        return;
    }

    RAWINPUTDEVICE device{};
    device.usUsagePage = 0x01;
    device.usUsage = 0x06;
    device.dwFlags = RIDEV_REMOVE;
    device.hwndTarget = nullptr;
    RegisterRawInputDevices(&device, 1, sizeof(device));

    hwnd_ = nullptr;
    active_ = false;
    deviceCache_.clear();
}

bool RawInputManager::isActive() const noexcept {
    return active_;
}

bool RawInputManager::handleNativeMessage(MSG* message, long* result) {
    if (!active_ || !message || message->message != WM_INPUT) {
        return false;
    }

    UINT requiredSize = 0;
    const auto rawInputHandle = reinterpret_cast<HRAWINPUT>(message->lParam);
    if (GetRawInputData(rawInputHandle, RID_INPUT, nullptr, &requiredSize, sizeof(RAWINPUTHEADER)) != 0 ||
        requiredSize == 0) {
        return false;
    }

    std::vector<BYTE> buffer(requiredSize);
    if (GetRawInputData(rawInputHandle, RID_INPUT, buffer.data(), &requiredSize, sizeof(RAWINPUTHEADER)) != requiredSize) {
        return false;
    }

    const auto* raw = reinterpret_cast<const RAWINPUT*>(buffer.data());
    if (raw->header.dwType != RIM_TYPEKEYBOARD) {
        return false;
    }

    if (!shouldAcceptDevice(raw->header.hDevice)) {
        if (result) {
            *result = 0;
        }
        return false;
    }

    const RAWKEYBOARD& keyboard = raw->data.keyboard;
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

    const std::wstring lowerName = ToLower(name);
    DeviceInfo info;
    info.name = std::move(name);
    info.virtualInjector = ContainsAnyVirtualToken(lowerName);
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
