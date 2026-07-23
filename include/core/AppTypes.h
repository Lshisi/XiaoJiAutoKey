#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

enum class InputDevice {
    Keyboard,
    Mouse,
};

enum class RepeatMode {
    Toggle,
    Hold,
    Independent,
    Sequence,
};

enum class InputSource {
    Physical,
    Injected,
    Repeater,
    Macro,
    Capture,
};

constexpr int kMinDelayMs = 1;
constexpr int kDefaultDelayMs = 10;
constexpr int kInputPulseWidthMs = 5;

struct KeyDescriptor {
    InputDevice device = InputDevice::Keyboard;
    std::uint32_t code = 0;

    [[nodiscard]] bool IsValid() const noexcept {
        return code != 0;
    }

    [[nodiscard]] bool operator==(const KeyDescriptor& other) const noexcept = default;
};

struct KeyBinding {
    std::uint64_t id = 0;
    bool enabled = true;
    KeyDescriptor key;
    RepeatMode mode = RepeatMode::Toggle;
    int delayMs = kDefaultDelayMs;
    bool suppressMixedMode = false;
};

enum class CloseAction {
    Ask,
    Minimize,
    Exit,
};

enum class DriverMode {
    Legacy,
    Hid,
    Usb,
};

enum class MacroTriggerMode {
    Toggle,
    Hold,
};

struct MacroStep {
    KeyDescriptor key;
    int delayAfterMs = kDefaultDelayMs;
};

struct MacroConfig {
    std::uint64_t id = 0;
    std::wstring name;
    std::vector<MacroStep> steps;
    std::vector<KeyDescriptor> pauseKeys;
    MacroTriggerMode triggerMode = MacroTriggerMode::Toggle;
    std::optional<KeyDescriptor> triggerKey;
    int loopCount = 0;
    bool enabled = true;
};

struct ProfileConfig {
    std::uint64_t id = 0;
    std::wstring name;
    bool enabled = true;
    std::optional<KeyDescriptor> controlHotkey;
    int defaultDelayMs = kDefaultDelayMs;
    bool mixedMode = false;
    bool wasdProtection = false;
    std::optional<KeyDescriptor> sequenceSwitchKey;
    std::vector<KeyDescriptor> pauseKeys;
    std::vector<KeyBinding> bindings;
};

struct AppConfig {
    bool beepEnabled = true;
    bool statusOverlayShowRepeat = false;
    bool statusOverlayShowMacro = false;
    bool statusOverlayLocked = false;
    bool statusOverlayPositionSaved = false;
    int statusOverlayX = 6;
    int statusOverlayY = 6;
    int statusOverlayOpacity = 70;
    CloseAction closeAction = CloseAction::Ask;
    DriverMode driverMode = DriverMode::Legacy;
    std::string faction = "cangjian";

    std::wstring customSoundOn;
    std::wstring customSoundOff;
    int soundVolume = 80;
    bool macroBeepEnabled = true;
    std::wstring macroSoundOn;
    std::wstring macroSoundOff;
    int macroSoundVolume = 80;

    std::uint64_t activeProfileId = 0;
    std::vector<ProfileConfig> profiles;
    std::uint64_t activeMacroId = 0;
    std::vector<MacroConfig> macros;
};

std::wstring ToWide(const std::string& text);
std::string ToUtf8(const std::wstring& text);

std::wstring GetKeyDisplayName(const KeyDescriptor& key);
std::wstring RepeatModeToText(RepeatMode mode);
bool IsToggleLikeRepeatMode(RepeatMode mode) noexcept;
std::wstring DeviceToText(InputDevice device);

constexpr std::uint32_t kNumpadEnterKeyCode = 0x0100000D;

KeyDescriptor NormalizeKeyDescriptor(const KeyDescriptor& key);
KeyDescriptor KeyboardHookKeyDescriptor(std::uint32_t vkCode, std::uint32_t scanCode, std::uint32_t flags);

std::wstring KeyDescriptorToStorage(const KeyDescriptor& key);
std::wstring KeyDescriptorToStorage(const std::optional<KeyDescriptor>& key);
std::optional<KeyDescriptor> StorageToKeyDescriptor(const std::wstring& text);
