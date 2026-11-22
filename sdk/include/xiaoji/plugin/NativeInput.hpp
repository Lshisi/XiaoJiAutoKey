#pragma once

#include <xiaoji/plugin/api.h>
#include <algorithm>
#include <climits>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>
#include <vector>
#include <Windows.h>

namespace xiaoji::plugin {
enum class InputDevice { Keyboard, Mouse };
enum class InputSource { Physical, Injected, Repeater, Macro, Capture };
constexpr std::uint8_t kKeyModifierCtrl = 1;
constexpr std::uint8_t kKeyModifierShift = 2;
constexpr std::uint8_t kKeyModifierAlt = 4;
constexpr int kInputPulseWidthMs = 5;
constexpr std::uint32_t kNumpadEnterKeyCode = 0x0100000D;

struct KeyDescriptor {
    InputDevice device = InputDevice::Keyboard;
    std::uint32_t code = 0;
    std::uint8_t modifiers = 0;
    bool IsValid() const noexcept { return code != 0; }
    bool operator==(const KeyDescriptor&) const noexcept = default;
};

inline std::wstring ToWide(const std::string& text) {
    if (text.empty() || text.size() > static_cast<size_t>(INT_MAX)) return {};
    const int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), nullptr, 0);
    if (count <= 0) return {};
    std::wstring value(count, L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), value.data(), count);
    return value;
}

inline std::string ToUtf8(const std::wstring& text) {
    if (text.empty() || text.size() > static_cast<size_t>(INT_MAX)) return {};
    const int count = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
    if (count <= 0) return {};
    std::string value(count, '\0');
    WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), value.data(), count, nullptr, nullptr);
    return value;
}

inline KeyDescriptor NormalizeKeyDescriptor(KeyDescriptor key) {
    key.modifiers &= kKeyModifierCtrl | kKeyModifierShift | kKeyModifierAlt;
    if (key.device != InputDevice::Keyboard) return key;
    if (key.code == VK_SHIFT) key.code = VK_LSHIFT;
    if (key.code == VK_CONTROL) key.code = VK_LCONTROL;
    if (key.code == VK_MENU) key.code = VK_LMENU;
    if (key.code == VK_LCONTROL || key.code == VK_RCONTROL || key.code == VK_LSHIFT
        || key.code == VK_RSHIFT || key.code == VK_LMENU || key.code == VK_RMENU) key.modifiers = 0;
    return key;
}

inline std::vector<KeyDescriptor> ExpandKeyComponents(const KeyDescriptor& key) {
    if (!key.IsValid()) return {};
    std::vector<KeyDescriptor> result;
    if (key.modifiers & kKeyModifierCtrl) result.push_back({InputDevice::Keyboard, VK_LCONTROL});
    if (key.modifiers & kKeyModifierShift) result.push_back({InputDevice::Keyboard, VK_LSHIFT});
    if (key.modifiers & kKeyModifierAlt) result.push_back({InputDevice::Keyboard, VK_LMENU});
    result.push_back({key.device, key.code, 0});
    return result;
}

inline std::wstring GetKeyDisplayName(const KeyDescriptor& key) {
    std::wstring prefix;
    if (key.modifiers & kKeyModifierCtrl) prefix += L"Ctrl+";
    if (key.modifiers & kKeyModifierShift) prefix += L"Shift+";
    if (key.modifiers & kKeyModifierAlt) prefix += L"Alt+";
    if (key.code == kNumpadEnterKeyCode) return prefix + L"Num Enter";
    wchar_t name[128]{};
    const LONG scan = static_cast<LONG>(MapVirtualKeyW(key.code, MAPVK_VK_TO_VSC)) << 16;
    if (GetKeyNameTextW(scan, name, 128) > 0) return prefix + name;
    return prefix + L"VK(" + std::to_wstring(key.code) + L")";
}

class InputDriver {
public:
    virtual ~InputDriver() = default;
    virtual bool IsReady() const noexcept = 0;
    virtual std::wstring GetLastError() const = 0;
    virtual std::wstring DiagnosticText() const { return {}; }
    virtual std::wstring KeySupportError(const KeyDescriptor&) const { return {}; }
    virtual bool SupportsPhysicalModifierIsolation() const { return true; }
    virtual bool SupportsPauseModifierRelay() const { return false; }
    virtual bool SupportsPhysicalKeyRelay(const KeyDescriptor&) const noexcept { return false; }
    virtual bool SendTap(const KeyDescriptor&, InputSource = InputSource::Injected) = 0;
    virtual bool SendDown(const KeyDescriptor&, InputSource = InputSource::Injected) = 0;
    virtual bool SendUp(const KeyDescriptor&, InputSource = InputSource::Injected) = 0;
    virtual bool SendHeldDown(const KeyDescriptor& key, InputSource source) { return SendDown(key, source); }
    virtual bool RestorePhysicalDown(const KeyDescriptor& key, InputSource source = InputSource::Injected) { return SendDown(key, source); }
    virtual bool SendMouseMove(int, int, InputSource = InputSource::Injected) { return false; }
    virtual bool SupportsMouseMove() const { return false; }
    virtual bool SendMouseWheel(int, InputSource = InputSource::Injected) { return false; }
    virtual bool SupportsMouseWheel() const { return false; }
    virtual void SetSourceActiveKeys(InputSource, const std::vector<KeyDescriptor>&) {}
    virtual void ReleaseSource(InputSource) { ReleaseAll(); }
    virtual void ReleaseAll() = 0;
    virtual void ClearAllStates() {}
    virtual bool ConsumeSelfInjected(const KeyDescriptor&, bool) { return false; }
    virtual bool ConsumeSelfInjectedRaw(const KeyDescriptor&, bool) { return false; }
    virtual bool ConsumeSelfInjectedWheel(int) { return false; }
};

inline const XjHostApi* hostApi = nullptr;
inline void Log(const std::string& line) noexcept {
    if (hostApi && hostApi->log) hostApi->log(hostApi->context, 1, line.c_str());
}
inline uint32_t CopyText(const std::string& text, char* buffer, uint32_t capacity) noexcept {
    if (text.size() >= UINT32_MAX) return 0;
    const uint32_t required = static_cast<uint32_t>(text.size() + 1);
    if (buffer && capacity) {
        const size_t count = std::min<size_t>(text.size(), capacity - 1);
        std::memcpy(buffer, text.data(), count);
        buffer[count] = '\0';
    }
    return required;
}
inline KeyDescriptor FromAbi(const XjKey& key) noexcept {
    return {key.device == XJ_MOUSE ? InputDevice::Mouse : InputDevice::Keyboard, key.code, static_cast<uint8_t>(key.modifiers)};
}

template<class Driver> struct DriverBridge {
    static void Bind(Driver* instance, XjDriverApi* output) {
        *output = {};
        output->size = sizeof(*output);
        output->abi_version = XJ_PLUGIN_ABI_VERSION;
        output->instance = instance;
        output->destroy = [](void* value) { delete static_cast<Driver*>(value); };
        output->is_ready = [](void* value) -> uint32_t { return static_cast<Driver*>(value)->IsReady(); };
        output->capabilities = [](void* value) -> uint64_t {
            const auto& driver = *static_cast<Driver*>(value);
            return XJ_CAP_KEYBOARD | XJ_CAP_MOUSE
                | (driver.SupportsMouseMove() ? XJ_CAP_MOVE : 0)
                | (driver.SupportsMouseWheel() ? XJ_CAP_WHEEL : 0)
                | (driver.SupportsPhysicalModifierIsolation() ? XJ_CAP_MODIFIER_ISOLATION : 0)
                | (driver.SupportsPauseModifierRelay() ? XJ_CAP_PAUSE_MODIFIER_RELAY : 0);
        };
        output->send = [](void* value, const XjInput* event) -> XjResult {
            if (!event || event->size < sizeof(XjInput) || event->source > XJ_SOURCE_CAPTURE) return XJ_INVALID_ARGUMENT;
            auto& driver = *static_cast<Driver*>(value);
            const auto key = FromAbi(event->key);
            const auto source = static_cast<InputSource>(event->source);
            try {
                bool ok = false;
                switch (event->action) {
                case XJ_TAP: ok = driver.SendTap(key, source); break;
                case XJ_DOWN: ok = driver.SendDown(key, source); break;
                case XJ_UP: ok = driver.SendUp(key, source); break;
                case XJ_HELD_DOWN: ok = driver.SendHeldDown(key, source); break;
                case XJ_RESTORE_PHYSICAL: ok = driver.RestorePhysicalDown(key, source); break;
                case XJ_MOVE: ok = driver.SendMouseMove(event->x, event->y, source); break;
                case XJ_WHEEL: ok = driver.SendMouseWheel(event->wheel_delta, source); break;
                default: return XJ_INVALID_ARGUMENT;
                }
                return ok ? XJ_OK : XJ_ERROR;
            } catch (...) { return XJ_ERROR; }
        };
        output->release_all = [](void* value) { static_cast<Driver*>(value)->ReleaseAll(); };
        output->clear_states = [](void* value) { static_cast<Driver*>(value)->ClearAllStates(); };
        output->consume = [](void* value, const XjInput* event, uint32_t channel) -> uint32_t {
            if (!event || event->size < sizeof(XjInput)) return 0;
            auto& driver = *static_cast<Driver*>(value);
            if (channel == XJ_CONSUME_WHEEL) return driver.ConsumeSelfInjectedWheel(event->wheel_delta);
            const auto key = FromAbi(event->key);
            const bool pressed = event->action != XJ_UP;
            return channel == XJ_CONSUME_RAW ? driver.ConsumeSelfInjectedRaw(key, pressed) : driver.ConsumeSelfInjected(key, pressed);
        };
        output->get_text = [](void* value, uint32_t kind, const XjKey* key, char* buffer, uint32_t capacity) -> uint32_t {
            auto& driver = *static_cast<Driver*>(value);
            if (kind == XJ_TEXT_DIAGNOSTIC) {
                if constexpr (requires { driver.DiagnosticText(); }) return CopyText(ToUtf8(driver.DiagnosticText()), buffer, capacity);
                return CopyText({}, buffer, capacity);
            }
            const auto text = kind == XJ_TEXT_ERROR ? driver.GetLastError()
                : kind == XJ_TEXT_KEY_SUPPORT && key ? driver.KeySupportError(FromAbi(*key)) : std::wstring();
            return CopyText(ToUtf8(text), buffer, capacity);
        };
        output->set_source_keys = [](void* value, uint32_t source, const XjKey* keys, uint32_t count) {
            if (source > XJ_SOURCE_CAPTURE || count > 4096 || (count && !keys)) return;
            std::vector<KeyDescriptor> converted;
            converted.reserve(count);
            for (uint32_t i = 0; i < count; ++i) converted.push_back(FromAbi(keys[i]));
            static_cast<Driver*>(value)->SetSourceActiveKeys(static_cast<InputSource>(source), converted);
        };
        output->release_source = [](void* value, uint32_t source) {
            if (source <= XJ_SOURCE_CAPTURE) static_cast<Driver*>(value)->ReleaseSource(static_cast<InputSource>(source));
        };
        output->supports_physical_key = [](void* value, const XjKey* key) -> uint32_t {
            return key && static_cast<Driver*>(value)->SupportsPhysicalKeyRelay(FromAbi(*key));
        };
    }
};
}
