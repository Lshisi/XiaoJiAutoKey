#pragma once

#include "AppTypes.h"

#include <Windows.h>
#include <cstdint>
#include <windowsx.h>

inline std::uint64_t EncodeKey(const KeyDescriptor& key) {
    return (static_cast<std::uint64_t>(key.device == InputDevice::Mouse ? 1 : 0) << 32)
        | (static_cast<std::uint64_t>(key.modifiers) << 40)
        | key.code;
}

inline KeyDescriptor DecodeKey(std::uint64_t value) {
    KeyDescriptor key;
    key.device = ((value >> 32) & 0x1) != 0 ? InputDevice::Mouse : InputDevice::Keyboard;
    key.code = static_cast<std::uint32_t>(value & 0xFFFFFFFFull);
    key.modifiers = static_cast<std::uint8_t>((value >> 40) & 0xFFull);
    return key;
}

inline KeyDescriptor MouseMessageToDescriptor(WPARAM wParam, const MSLLHOOKSTRUCT& info) {
    switch (wParam) {
    case WM_LBUTTONDOWN: case WM_LBUTTONUP: return {InputDevice::Mouse, VK_LBUTTON};
    case WM_RBUTTONDOWN: case WM_RBUTTONUP: return {InputDevice::Mouse, VK_RBUTTON};
    case WM_MBUTTONDOWN: case WM_MBUTTONUP: return {InputDevice::Mouse, VK_MBUTTON};
    case WM_XBUTTONDOWN: case WM_XBUTTONUP:
        return {InputDevice::Mouse, static_cast<std::uint32_t>(HIWORD(info.mouseData) == XBUTTON1 ? VK_XBUTTON1 : VK_XBUTTON2)};
    default: return {};
    }
}

inline bool IsMouseDownMessage(WPARAM wParam) {
    return wParam == WM_LBUTTONDOWN || wParam == WM_RBUTTONDOWN || wParam == WM_MBUTTONDOWN || wParam == WM_XBUTTONDOWN;
}

inline bool IsMouseUpMessage(WPARAM wParam) {
    return wParam == WM_LBUTTONUP || wParam == WM_RBUTTONUP || wParam == WM_MBUTTONUP || wParam == WM_XBUTTONUP;
}
