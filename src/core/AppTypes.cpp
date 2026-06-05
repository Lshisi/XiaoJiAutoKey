#include "AppTypes.h"

#include <Windows.h>

#include <sstream>

namespace {

bool IsExtendedKey(std::uint32_t vk) {
    switch (vk) {
    case VK_INSERT:
    case VK_DELETE:
    case VK_HOME:
    case VK_END:
    case VK_PRIOR:
    case VK_NEXT:
    case VK_LEFT:
    case VK_RIGHT:
    case VK_UP:
    case VK_DOWN:
    case VK_DIVIDE:
    case VK_NUMLOCK:
    case VK_RCONTROL:
    case VK_RMENU:
    case VK_SNAPSHOT:
        return true;
    default:
        return false;
    }
}

std::wstring MouseButtonName(std::uint32_t vk) {
    switch (vk) {
    case VK_LBUTTON:
        return L"鼠标左键";
    case VK_RBUTTON:
        return L"鼠标右键";
    case VK_MBUTTON:
        return L"鼠标中键";
    case VK_XBUTTON1:
        return L"鼠标侧键1";
    case VK_XBUTTON2:
        return L"鼠标侧键2";
    default:
        return L"未知鼠标键";
    }
}

std::wstring NumpadKeyName(std::uint32_t vk) {
    if (vk >= VK_NUMPAD0 && vk <= VK_NUMPAD9) {
        return L"Num " + std::to_wstring(vk - VK_NUMPAD0);
    }

    switch (vk) {
    case VK_NUMLOCK:
        return L"Num Lock";
    case VK_DIVIDE:
        return L"Num /";
    case VK_MULTIPLY:
        return L"Num *";
    case VK_SUBTRACT:
        return L"Num -";
    case VK_ADD:
        return L"Num +";
    case VK_DECIMAL:
        return L"Num .";
    default:
        return {};
    }
}

std::uint32_t NormalizeKeyboardVirtualKey(std::uint32_t vk) {
    switch (vk) {
    case VK_SHIFT:
        return VK_LSHIFT;
    case VK_CONTROL:
        return VK_LCONTROL;
    case VK_MENU:
        return VK_LMENU;
    default:
        return vk;
    }
}

}  // namespace

std::wstring ToWide(const std::string& text) {
    if (text.empty()) {
        return {};
    }
    const int size = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), nullptr, 0);
    std::wstring result(size, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), result.data(), size);
    return result;
}

std::string ToUtf8(const std::wstring& text) {
    if (text.empty()) {
        return {};
    }
    const int size = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
    std::string result(size, '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), result.data(), size, nullptr, nullptr);
    return result;
}

std::wstring GetKeyDisplayName(const KeyDescriptor& key) {
    if (!key.IsValid()) {
        return L"None";
    }

    if (key.device == InputDevice::Mouse) {
        return MouseButtonName(key.code);
    }

    if (key.code == kNumpadEnterKeyCode) {
        return L"Num Enter";
    }

    if (const auto numpadName = NumpadKeyName(key.code); !numpadName.empty()) {
        return numpadName;
    }

    if (key.code == VK_PAUSE) {
        return L"Pause";
    }

    LONG scanCode = static_cast<LONG>(MapVirtualKeyW(key.code, MAPVK_VK_TO_VSC)) << 16;
    if (IsExtendedKey(key.code)) {
        scanCode |= 1 << 24;
    }

    wchar_t buffer[128] = {};
    if (GetKeyNameTextW(scanCode, buffer, static_cast<int>(std::size(buffer))) > 0) {
        return buffer;
    }

    std::wstringstream stream;
    stream << L"VK(" << key.code << L")";
    return stream.str();
}

std::wstring RepeatModeToText(RepeatMode mode) {
    return mode == RepeatMode::Toggle ? L"开关" : L"按压";
}

std::wstring DeviceToText(InputDevice device) {
    return device == InputDevice::Keyboard ? L"键盘" : L"鼠标";
}

std::wstring KeyDescriptorToStorage(const KeyDescriptor& key) {
    return KeyDescriptorToStorage(std::optional<KeyDescriptor>(key));
}

KeyDescriptor NormalizeKeyDescriptor(const KeyDescriptor& key) {
    if (key.device != InputDevice::Keyboard) {
        return key;
    }
    return {InputDevice::Keyboard, NormalizeKeyboardVirtualKey(key.code)};
}

KeyDescriptor KeyboardHookKeyDescriptor(std::uint32_t vkCode, std::uint32_t scanCode, std::uint32_t flags) {
    switch (vkCode) {
    case VK_SHIFT: {
        const auto mapped = MapVirtualKeyW(scanCode, MAPVK_VSC_TO_VK_EX);
        if (mapped == VK_LSHIFT || mapped == VK_RSHIFT) {
            return {InputDevice::Keyboard, mapped};
        }
        return {InputDevice::Keyboard, VK_LSHIFT};
    }
    case VK_CONTROL:
        return {InputDevice::Keyboard, static_cast<std::uint32_t>((flags & LLKHF_EXTENDED) ? VK_RCONTROL : VK_LCONTROL)};
    case VK_MENU:
        return {InputDevice::Keyboard, static_cast<std::uint32_t>((flags & LLKHF_EXTENDED) ? VK_RMENU : VK_LMENU)};
    case VK_RETURN:
        return {InputDevice::Keyboard, (flags & LLKHF_EXTENDED) ? kNumpadEnterKeyCode : VK_RETURN};
    default:
        return NormalizeKeyDescriptor({InputDevice::Keyboard, vkCode});
    }
}

std::wstring KeyDescriptorToStorage(const std::optional<KeyDescriptor>& key) {
    if (!key.has_value()) {
        return L"";
    }

    const KeyDescriptor normalized = NormalizeKeyDescriptor(*key);
    std::wstringstream stream;
    stream << (normalized.device == InputDevice::Keyboard ? L"K:" : L"M:") << normalized.code;
    return stream.str();
}

std::optional<KeyDescriptor> StorageToKeyDescriptor(const std::wstring& text) {
    if (text.size() < 3 || text[1] != L':') {
        return std::nullopt;
    }

    KeyDescriptor descriptor;
    descriptor.device = (text[0] == L'M') ? InputDevice::Mouse : InputDevice::Keyboard;

    try {
        descriptor.code = static_cast<std::uint32_t>(std::stoul(text.substr(2)));
    } catch (...) {
        return std::nullopt;
    }

    descriptor = NormalizeKeyDescriptor(descriptor);
    if (!descriptor.IsValid()) {
        return std::nullopt;
    }
    return descriptor;
}
