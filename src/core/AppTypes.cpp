#include "AppTypes.h"

#include <Windows.h>

#include <optional>
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
        return L"\u5de6\u952e";
    case VK_RBUTTON:
        return L"\u53f3\u952e";
    case VK_MBUTTON:
        return L"\u4e2d\u952e";
    case VK_XBUTTON1:
        return L"\u4fa7\u952e1";
    case VK_XBUTTON2:
        return L"\u4fa7\u952e2";
    default:
        return L"\u672a\u77e5\u9f20\u6807\u952e";
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

std::wstring KeyboardKeyName(std::uint32_t vk) {
    if (vk == kNumpadEnterKeyCode) return L"Num Enter";
    if (const auto numpadName = NumpadKeyName(vk); !numpadName.empty()) return numpadName;
    if (vk == VK_PAUSE) return L"Pause";

    LONG scanCode = static_cast<LONG>(MapVirtualKeyW(vk, MAPVK_VK_TO_VSC)) << 16;
    if (IsExtendedKey(vk)) scanCode |= 1 << 24;

    wchar_t buffer[128] = {};
    if (GetKeyNameTextW(scanCode, buffer, static_cast<int>(std::size(buffer))) > 0) {
        return buffer;
    }

    std::wstringstream stream;
    stream << L"VK(" << vk << L")";
    return stream.str();
}

std::wstring ModifierStorageSuffix(std::uint8_t modifiers) {
    if (modifiers == 0) return {};
    std::wstring result = L"+";
    bool first = true;
    const auto append = [&](const wchar_t* name) {
        if (!first) result += L"+";
        result += name;
        first = false;
    };
    if ((modifiers & kKeyModifierCtrl) != 0) append(L"Ctrl");
    if ((modifiers & kKeyModifierShift) != 0) append(L"Shift");
    if ((modifiers & kKeyModifierAlt) != 0) append(L"Alt");
    return result;
}

std::optional<std::uint8_t> ModifiersFromStorageSuffix(const std::wstring& text) {
    if (text.empty() || text.back() == L'+') return std::nullopt;
    std::uint8_t modifiers = 0;
    std::size_t start = 0;
    while (start <= text.size()) {
        const auto end = text.find(L'+', start);
        const auto part = text.substr(start, end == std::wstring::npos
            ? std::wstring::npos : end - start);
        if (part == L"Ctrl") {
            if ((modifiers & kKeyModifierCtrl) != 0) return std::nullopt;
            modifiers |= kKeyModifierCtrl;
        } else if (part == L"Shift") {
            if ((modifiers & kKeyModifierShift) != 0) return std::nullopt;
            modifiers |= kKeyModifierShift;
        } else if (part == L"Alt") {
            if ((modifiers & kKeyModifierAlt) != 0) return std::nullopt;
            modifiers |= kKeyModifierAlt;
        }
        else return std::nullopt;
        if (end == std::wstring::npos) break;
        start = end + 1;
    }
    return modifiers;
}

}  // namespace

std::wstring ToWide(const std::string& text) {
    if (text.empty()) return {};
    const int size = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), nullptr, 0);
    std::wstring result(size, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), result.data(), size);
    return result;
}

std::string ToUtf8(const std::wstring& text) {
    if (text.empty()) return {};
    const int size = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()),
        nullptr, 0, nullptr, nullptr);
    std::string result(size, '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()),
        result.data(), size, nullptr, nullptr);
    return result;
}

std::wstring GetKeyDisplayName(const KeyDescriptor& key) {
    if (!key.IsValid()) return L"None";
    const std::wstring primary = key.device == InputDevice::Mouse
        ? MouseButtonName(key.code)
        : KeyboardKeyName(key.code);
    return ModifierPrefix(key.modifiers) + primary;
}

bool IsKeyboardModifierKey(const KeyDescriptor& key) noexcept {
    return key.device == InputDevice::Keyboard
        && (key.code == VK_LCONTROL || key.code == VK_RCONTROL
            || key.code == VK_LSHIFT || key.code == VK_RSHIFT
            || key.code == VK_LMENU || key.code == VK_RMENU);
}

std::wstring ModifierPrefix(std::uint8_t modifiers) {
    if (modifiers == 0) return {};
    std::wstring result;
    if ((modifiers & kKeyModifierCtrl) != 0) result += L"Ctrl+";
    if ((modifiers & kKeyModifierShift) != 0) result += L"Shift+";
    if ((modifiers & kKeyModifierAlt) != 0) result += L"Alt+";
    return result;
}

bool SamePrimaryKey(const KeyDescriptor& left, const KeyDescriptor& right) noexcept {
    return left.device == right.device && left.code == right.code;
}

bool KeyMatchesEvent(const KeyDescriptor& binding, const KeyDescriptor& event) noexcept {
    return SamePrimaryKey(binding, event) && binding.modifiers == event.modifiers;
}

std::vector<KeyDescriptor> ExpandKeyComponents(const KeyDescriptor& key) {
    if (!key.IsValid()) return {};

    std::vector<KeyDescriptor> components;
    if ((key.modifiers & kKeyModifierCtrl) != 0)
        components.push_back({InputDevice::Keyboard, VK_LCONTROL});
    if ((key.modifiers & kKeyModifierShift) != 0)
        components.push_back({InputDevice::Keyboard, VK_LSHIFT});
    if ((key.modifiers & kKeyModifierAlt) != 0)
        components.push_back({InputDevice::Keyboard, VK_LMENU});

    KeyDescriptor primary = key;
    primary.modifiers = 0;
    components.push_back(std::move(primary));
    return components;
}

bool IsReservedKeyCombination(const KeyDescriptor& key) noexcept {
    if (key.device != InputDevice::Keyboard || key.modifiers == 0) return false;
    if (key.modifiers == (kKeyModifierCtrl | kKeyModifierAlt) && key.code == VK_DELETE) return true;
    if (key.modifiers == (kKeyModifierCtrl | kKeyModifierShift) && key.code == VK_ESCAPE) return true;
    if (key.modifiers == kKeyModifierAlt && key.code == VK_TAB) return true;
    return false;
}

std::wstring RepeatModeToText(RepeatMode mode) {
    switch (mode) {
    case RepeatMode::Toggle:
        return L"\u5f00\u5173";
    case RepeatMode::Hold:
        return L"\u6309\u538b";
    case RepeatMode::Independent:
        return L"\u72ec\u7acb";
    case RepeatMode::Sequence:
        return L"\u5207\u6362";
    case RepeatMode::LongPress:
        return L"\u957f\u6309";
    default:
        return L"\u672a\u77e5";
    }
}

bool IsToggleLikeRepeatMode(RepeatMode mode) noexcept {
    return mode == RepeatMode::Toggle
        || mode == RepeatMode::Independent
        || mode == RepeatMode::Sequence
        || mode == RepeatMode::LongPress;
}

std::wstring DeviceToText(InputDevice device) {
    return device == InputDevice::Keyboard ? L"\u952e\u76d8" : L"\u9f20\u6807";
}

std::wstring KeyDescriptorToStorage(const KeyDescriptor& key) {
    return KeyDescriptorToStorage(std::optional<KeyDescriptor>(key));
}

KeyDescriptor NormalizeKeyDescriptor(const KeyDescriptor& key) {
    if (key.device != InputDevice::Keyboard) {
        KeyDescriptor normalized = key;
        normalized.modifiers &= kKeyModifierCtrl | kKeyModifierShift | kKeyModifierAlt;
        return normalized;
    }

    KeyDescriptor normalized{InputDevice::Keyboard, NormalizeKeyboardVirtualKey(key.code)};
    if (IsKeyboardModifierKey(normalized)) normalized.modifiers = 0;
    else normalized.modifiers = key.modifiers
        & (kKeyModifierCtrl | kKeyModifierShift | kKeyModifierAlt);
    return normalized;
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
        return {InputDevice::Keyboard,
            static_cast<std::uint32_t>((flags & LLKHF_EXTENDED) ? VK_RCONTROL : VK_LCONTROL)};
    case VK_MENU:
        return {InputDevice::Keyboard,
            static_cast<std::uint32_t>((flags & LLKHF_EXTENDED) ? VK_RMENU : VK_LMENU)};
    case VK_RETURN:
        return {InputDevice::Keyboard,
            (flags & LLKHF_EXTENDED) ? kNumpadEnterKeyCode : VK_RETURN};
    default:
        return NormalizeKeyDescriptor({InputDevice::Keyboard, vkCode});
    }
}

std::wstring KeyDescriptorToStorage(const std::optional<KeyDescriptor>& key) {
    if (!key.has_value()) return {};

    const KeyDescriptor normalized = NormalizeKeyDescriptor(*key);
    std::wstringstream stream;
    stream << (normalized.device == InputDevice::Keyboard ? L"K:" : L"M:")
        << normalized.code << ModifierStorageSuffix(normalized.modifiers);
    return stream.str();
}

std::optional<KeyDescriptor> StorageToKeyDescriptor(const std::wstring& text) {
    if (text.size() < 3 || text[1] != L':') return std::nullopt;

    const auto modifierStart = text.find(L'+', 2);
    const auto codeText = text.substr(2, modifierStart == std::wstring::npos
        ? std::wstring::npos : modifierStart - 2);
    if (codeText.empty()) return std::nullopt;

    KeyDescriptor descriptor;
    descriptor.device = text[0] == L'M' ? InputDevice::Mouse : InputDevice::Keyboard;
    try {
        descriptor.code = static_cast<std::uint32_t>(std::stoul(codeText));
    } catch (...) {
        return std::nullopt;
    }
    if (modifierStart != std::wstring::npos) {
        const auto modifiers = ModifiersFromStorageSuffix(text.substr(modifierStart + 1));
        if (!modifiers.has_value()) return std::nullopt;
        descriptor.modifiers = *modifiers;
    }

    descriptor = NormalizeKeyDescriptor(descriptor);
    return descriptor.IsValid() ? std::optional<KeyDescriptor>{descriptor} : std::nullopt;
}
