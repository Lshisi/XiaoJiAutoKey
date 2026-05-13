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
    return mode == RepeatMode::Toggle ? L"开关模式" : L"按压模式";
}

std::wstring DeviceToText(InputDevice device) {
    return device == InputDevice::Keyboard ? L"键盘" : L"鼠标";
}

std::wstring KeyDescriptorToStorage(const KeyDescriptor& key) {
    return KeyDescriptorToStorage(std::optional<KeyDescriptor>(key));
}

std::wstring KeyDescriptorToStorage(const std::optional<KeyDescriptor>& key) {
    if (!key.has_value()) {
        return L"";
    }

    std::wstringstream stream;
    stream << (key->device == InputDevice::Keyboard ? L"K:" : L"M:") << key->code;
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

    if (!descriptor.IsValid()) {
        return std::nullopt;
    }
    return descriptor;
}
