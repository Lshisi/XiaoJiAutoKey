#include "XiaojiKeyMap.h"

#include <Windows.h>

namespace {
std::uint8_t Modifier(std::uint32_t code) {
    switch (code) {
    case VK_LCONTROL: return 0x01;
    case VK_LSHIFT: return 0x02;
    case VK_LMENU: return 0x04;
    case VK_LWIN: return 0x08;
    case VK_RCONTROL: return 0x10;
    case VK_RSHIFT: return 0x20;
    case VK_RMENU: return 0x40;
    case VK_RWIN: return 0x80;
    default: return 0;
    }
}

std::uint16_t Consumer(std::uint32_t code) {
    switch (code) {
    case VK_VOLUME_MUTE: return 0xE2;
    case VK_VOLUME_UP: return 0xE9;
    case VK_VOLUME_DOWN: return 0xEA;
    case VK_MEDIA_NEXT_TRACK: return 0xB5;
    case VK_MEDIA_PREV_TRACK: return 0xB6;
    case VK_MEDIA_STOP: return 0xB7;
    case VK_MEDIA_PLAY_PAUSE: return 0xCD;
    case VK_BROWSER_BACK: return 0x224;
    case VK_BROWSER_FORWARD: return 0x225;
    case VK_BROWSER_REFRESH: return 0x227;
    case VK_BROWSER_STOP: return 0x226;
    case VK_BROWSER_SEARCH: return 0x221;
    case VK_BROWSER_FAVORITES: return 0x22A;
    case VK_BROWSER_HOME: return 0x223;
    case VK_LAUNCH_MAIL: return 0x18A;
    case VK_LAUNCH_MEDIA_SELECT: return 0x183;
    case VK_LAUNCH_APP1: return 0x194;
    case VK_LAUNCH_APP2: return 0x192;
    case VK_SLEEP: return 0x32;
    default: return 0;
    }
}

HKL ForegroundLayout() {
    const DWORD thread = GetWindowThreadProcessId(GetForegroundWindow(), nullptr);
    return GetKeyboardLayout(thread);
}

std::uint8_t Keyboard(std::uint32_t code) {
    if (code >= VK_F1 && code <= VK_F12) return static_cast<std::uint8_t>(0x3A + code - VK_F1);
    if (code >= VK_F13 && code <= VK_F24) return static_cast<std::uint8_t>(0x68 + code - VK_F13);
    if (code >= VK_NUMPAD1 && code <= VK_NUMPAD9) return static_cast<std::uint8_t>(0x59 + code - VK_NUMPAD1);
    const auto layout = ForegroundLayout();
    switch (code) {
    case VK_RETURN: return 0x28;
    case VK_ESCAPE: return 0x29;
    case VK_BACK: return 0x2A;
    case VK_TAB: return 0x2B;
    case VK_SPACE: return 0x2C;
    case VK_CAPITAL: return 0x39;
    case VK_SCROLL: return 0x47;
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
    case VK_DIVIDE: return 0x54;
    case VK_MULTIPLY: return 0x55;
    case VK_SUBTRACT: return 0x56;
    case VK_ADD: return 0x57;
    case VK_NUMPAD0: return 0x62;
    case VK_DECIMAL: return 0x63;
    case VK_APPS: return 0x65;
    case kNumpadEnterKeyCode: return 0x58;
    case VK_PAUSE: return 0x48;
    case VK_NUMLOCK: return 0x53;
    case VK_SNAPSHOT: case VK_PRINT: return 0x46;
    case VK_CANCEL: return 0x9B;
    case VK_SELECT: return 0x77;
    case VK_EXECUTE: return 0x74;
    case VK_HELP: return 0x75;
    case VK_SEPARATOR: return 0x85;
    case VK_OEM_CLEAR: return 0x9C;
    case VK_ATTN: return 0x9A;
    case VK_CRSEL: return 0xA3;
    case VK_EXSEL: return 0xA4;
    case VK_CONVERT: return 0x8A;
    case VK_NONCONVERT: return 0x8B;
    case VK_KANA:
        return PRIMARYLANGID(LOWORD(reinterpret_cast<ULONG_PTR>(layout))) == LANG_KOREAN ? 0x90 : 0x88;
    case VK_HANJA:
        return PRIMARYLANGID(LOWORD(reinterpret_cast<ULONG_PTR>(layout))) == LANG_JAPANESE ? 0x94 : 0x91;
    default: break;
    }
    const UINT scan = MapVirtualKeyExW(code, MAPVK_VK_TO_VSC_EX, layout);
    if ((scan & 0xFF00) == 0xE000) {
        switch (scan & 0xFF) {
        case 0x1C: return 0x58;
        case 0x35: return 0x54;
        case 0x37: return 0x46;
        case 0x47: return 0x4A;
        case 0x48: return 0x52;
        case 0x49: return 0x4B;
        case 0x4B: return 0x50;
        case 0x4D: return 0x4F;
        case 0x4F: return 0x4D;
        case 0x50: return 0x51;
        case 0x51: return 0x4E;
        case 0x52: return 0x49;
        case 0x53: return 0x4C;
        case 0x5D: return 0x65;
        default: return 0;
        }
    }
    // Set-1 physical scan codes to Keyboard/Keypad usages. Mapping VK through
    // the foreground layout preserves AZERTY/QWERTZ and OEM key positions.
    static constexpr std::uint8_t set1[] = {
        0, 0x29, 0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23,
        0x24, 0x25, 0x26, 0x27, 0x2D, 0x2E, 0x2A, 0x2B,
        0x14, 0x1A, 0x08, 0x15, 0x17, 0x1C, 0x18, 0x0C,
        0x12, 0x13, 0x2F, 0x30, 0x28, 0, 0x04, 0x16,
        0x07, 0x09, 0x0A, 0x0B, 0x0D, 0x0E, 0x0F, 0x33,
        0x34, 0x35, 0, 0x31, 0x1D, 0x1B, 0x06, 0x19,
        0x05, 0x11, 0x10, 0x36, 0x37, 0x38, 0, 0x55,
        0, 0x2C, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E,
        0x3F, 0x40, 0x41, 0x42, 0x43, 0x53, 0x47, 0x5F,
        0x60, 0x61, 0x56, 0x5C, 0x5D, 0x5E, 0x57, 0x59,
        0x5A, 0x5B, 0x62, 0x63, 0, 0, 0x64, 0x44, 0x45,
    };
    if (scan < sizeof(set1)) return set1[scan];
    switch (scan) {
    case 0x70: return 0x88;
    case 0x73: return 0x87;
    case 0x79: return 0x8A;
    case 0x7B: return 0x8B;
    case 0x7D: return 0x89;
    default: return 0;
    }
}
}

XiaojiKeys::Output XiaojiKeys::Resolve(const KeyDescriptor& rawKey) {
    const auto key = NormalizeKeyDescriptor(rawKey);
    Output result;
    if (key.device == InputDevice::Mouse) {
        switch (key.code) {
        case VK_LBUTTON: result.mouseButton = 1; break;
        case VK_RBUTTON: result.mouseButton = 2; break;
        case VK_MBUTTON: result.mouseButton = 4; break;
        case VK_XBUTTON1: result.mouseButton = 8; break;
        case VK_XBUTTON2: result.mouseButton = 16; break;
        default: break;
        }
    } else if ((result.modifier = Modifier(key.code)) == 0
        && (result.consumerUsage = Consumer(key.code)) == 0) result.keyboardUsage = Keyboard(key.code);
    return result;
}
