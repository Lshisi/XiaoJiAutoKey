#pragma once

#include "AppTypes.h"
#include <Qt>

inline KeyDescriptor PreviewKeyboardKey(int key, Qt::KeyboardModifiers modifiers = {}) {
    quint32 code = 0;
    if (key >= Qt::Key_A && key <= Qt::Key_Z) code = key;
    else if (key >= Qt::Key_0 && key <= Qt::Key_9)
        code = modifiers.testFlag(Qt::KeypadModifier) ? 0x60 + key - Qt::Key_0 : key;
    else if (key >= Qt::Key_F1 && key <= Qt::Key_F24) code = 0x70 + key - Qt::Key_F1;
    else {
        switch (key) {
        case Qt::Key_Escape: code = 0x1b; break;
        case Qt::Key_Tab: case Qt::Key_Backtab: code = 9; break;
        case Qt::Key_Backspace: code = 8; break;
        case Qt::Key_Return: code = 13; break;
        case Qt::Key_Enter: code = kNumpadEnterKeyCode; break;
        case Qt::Key_Space: code = 32; break;
        case Qt::Key_Shift: code = 0x10; break;
        case Qt::Key_Control: code = 0x11; break;
        case Qt::Key_Alt: code = 0x12; break;
        case Qt::Key_Meta: code = 0x5b; break;
        case Qt::Key_CapsLock: code = 0x14; break;
        case Qt::Key_NumLock: code = 0x90; break;
        case Qt::Key_ScrollLock: code = 0x91; break;
        case Qt::Key_Print: code = 0x2c; break;
        case Qt::Key_Pause: code = 0x13; break;
        case Qt::Key_Insert: code = 0x2d; break;
        case Qt::Key_Delete: code = 0x2e; break;
        case Qt::Key_Home: code = 0x24; break;
        case Qt::Key_End: code = 0x23; break;
        case Qt::Key_PageUp: code = 0x21; break;
        case Qt::Key_PageDown: code = 0x22; break;
        case Qt::Key_Left: code = 0x25; break;
        case Qt::Key_Up: code = 0x26; break;
        case Qt::Key_Right: code = 0x27; break;
        case Qt::Key_Down: code = 0x28; break;
        case Qt::Key_Menu: code = 0x5d; break;
        case Qt::Key_QuoteLeft: case Qt::Key_AsciiTilde: code = 0xc0; break;
        case Qt::Key_Minus: case Qt::Key_Underscore: code = modifiers.testFlag(Qt::KeypadModifier) ? 0x6d : 0xbd; break;
        case Qt::Key_Equal: case Qt::Key_Plus: code = modifiers.testFlag(Qt::KeypadModifier) ? 0x6b : 0xbb; break;
        case Qt::Key_BracketLeft: case Qt::Key_BraceLeft: code = 0xdb; break;
        case Qt::Key_BracketRight: case Qt::Key_BraceRight: code = 0xdd; break;
        case Qt::Key_Backslash: case Qt::Key_Bar: code = 0xdc; break;
        case Qt::Key_Semicolon: case Qt::Key_Colon: code = 0xba; break;
        case Qt::Key_Apostrophe: case Qt::Key_QuoteDbl: code = 0xde; break;
        case Qt::Key_Comma: case Qt::Key_Less: code = 0xbc; break;
        case Qt::Key_Period: case Qt::Key_Greater: code = modifiers.testFlag(Qt::KeypadModifier) ? 0x6e : 0xbe; break;
        case Qt::Key_Slash: case Qt::Key_Question: code = modifiers.testFlag(Qt::KeypadModifier) ? 0x6f : 0xbf; break;
        case Qt::Key_Asterisk: code = modifiers.testFlag(Qt::KeypadModifier) ? 0x6a : 0x38; break;
        case Qt::Key_Exclam: code = 0x31; break;
        case Qt::Key_At: code = 0x32; break;
        case Qt::Key_NumberSign: code = 0x33; break;
        case Qt::Key_Dollar: code = 0x34; break;
        case Qt::Key_Percent: code = 0x35; break;
        case Qt::Key_AsciiCircum: code = 0x36; break;
        case Qt::Key_Ampersand: code = 0x37; break;
        case Qt::Key_ParenLeft: code = 0x39; break;
        case Qt::Key_ParenRight: code = 0x30; break;
        default: code = static_cast<quint32>(key); break;
        }
    }
    KeyDescriptor descriptor{InputDevice::Keyboard, code};
    if (key != Qt::Key_Shift && key != Qt::Key_Control && key != Qt::Key_Alt) {
        if (modifiers.testFlag(Qt::ControlModifier)) descriptor.modifiers |= kKeyModifierCtrl;
        if (modifiers.testFlag(Qt::ShiftModifier)) descriptor.modifiers |= kKeyModifierShift;
        if (modifiers.testFlag(Qt::AltModifier)) descriptor.modifiers |= kKeyModifierAlt;
    }
    return descriptor;
}

inline KeyDescriptor PreviewMouseKey(Qt::MouseButton button, Qt::KeyboardModifiers modifiers = {}) {
    KeyDescriptor key{InputDevice::Mouse, 0};
    switch (button) {
    case Qt::LeftButton: key.code = 1; break;
    case Qt::RightButton: key.code = 2; break;
    case Qt::MiddleButton: key.code = 4; break;
    case Qt::XButton1: key.code = 5; break;
    case Qt::XButton2: key.code = 6; break;
    default: break;
    }
    if (modifiers.testFlag(Qt::ControlModifier)) key.modifiers |= kKeyModifierCtrl;
    if (modifiers.testFlag(Qt::ShiftModifier)) key.modifiers |= kKeyModifierShift;
    if (modifiers.testFlag(Qt::AltModifier)) key.modifiers |= kKeyModifierAlt;
    return key;
}
