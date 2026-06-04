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
};

enum class InputSource {
    Physical,
    Injected,
    Capture,
};

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
    int delayMs = 10;
};

enum class CloseAction {
    Ask,
    Minimize,
    Exit,
};

enum class MacroTriggerMode {
    Toggle,
    Hold,
};

struct MacroStep {
    KeyDescriptor key;
    int delayAfterMs = 50;
};

struct MacroConfig {
    std::uint64_t id = 0;
    std::wstring name;
    std::vector<MacroStep> steps;
    MacroTriggerMode triggerMode = MacroTriggerMode::Toggle;
    std::optional<KeyDescriptor> triggerKey;
    int loopCount = 0;
    bool enabled = true;
};

struct ProfileConfig {
    std::uint64_t id = 0;
    std::wstring name;
    std::optional<KeyDescriptor> controlHotkey;
    int defaultDelayMs = 10;
    bool mixedMode = false;
    std::vector<KeyDescriptor> pauseKeys;
    std::vector<KeyBinding> bindings;
    std::vector<MacroConfig> macros;
};

struct AppConfig {
    bool beepEnabled = true;
    CloseAction closeAction = CloseAction::Ask;
    std::string faction = "none";

    std::wstring customSoundOn;
    std::wstring customSoundOff;
    int soundVolume = 80;
    bool macroBeepEnabled = true;
    std::wstring macroSoundOn;
    std::wstring macroSoundOff;
    int macroSoundVolume = 80;

    std::uint64_t activeProfileId = 0;
    std::vector<ProfileConfig> profiles;
};

std::wstring ToWide(const std::string& text);
std::string ToUtf8(const std::wstring& text);

std::wstring GetKeyDisplayName(const KeyDescriptor& key);
std::wstring RepeatModeToText(RepeatMode mode);
std::wstring DeviceToText(InputDevice device);

std::wstring KeyDescriptorToStorage(const KeyDescriptor& key);
std::wstring KeyDescriptorToStorage(const std::optional<KeyDescriptor>& key);
std::optional<KeyDescriptor> StorageToKeyDescriptor(const std::wstring& text);
