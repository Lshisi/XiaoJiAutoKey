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
    Physical,   // 用户真实按键
    Injected,   // 自己SendTap
    Capture     // 录制状态
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
    int delayMs = 100;
};

struct AppConfig {
    std::optional<KeyDescriptor> controlHotkey;
    int defaultDelayMs = 100;
    bool beepEnabled = true;
    std::vector<KeyBinding> bindings;
};

std::wstring ToWide(const std::string& text);
std::string ToUtf8(const std::wstring& text);

std::wstring GetKeyDisplayName(const KeyDescriptor& key);
std::wstring RepeatModeToText(RepeatMode mode);
std::wstring DeviceToText(InputDevice device);

std::wstring KeyDescriptorToStorage(const KeyDescriptor& key);
std::wstring KeyDescriptorToStorage(const std::optional<KeyDescriptor>& key);
std::optional<KeyDescriptor> StorageToKeyDescriptor(const std::wstring& text);
