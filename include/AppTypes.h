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
    int delayMs = 10;
};

enum class CloseAction {
    Ask,        // 每次询问
    Minimize,   // 最小化到托盘
    Exit,       // 直接退出
};

// ── 宏相关类型 ─────────────────────────────────
enum class MacroTriggerMode {
    Toggle,     // 按一次开始，再按一次停止
    Hold,       // 按住执行，松开停止
};

struct MacroStep {
    KeyDescriptor key;
    int delayAfterMs = 50;  // 该步骤执行后的等待时间(ms)
};

struct MacroConfig {
    std::uint64_t id = 0;
    std::wstring name;                          // 宏名称
    std::vector<MacroStep> steps;               // 按键序列
    MacroTriggerMode triggerMode = MacroTriggerMode::Toggle;
    std::optional<KeyDescriptor> triggerKey;     // 触发/启停快捷键
    int loopCount = 0;                          // 0 = 无限循环
    bool enabled = true;
};

// ── 配置方案（Profile）─────────────────────────
struct ProfileConfig {
    std::uint64_t id = 0;
    std::wstring name;
    std::optional<KeyDescriptor> controlHotkey;
    int defaultDelayMs = 10;
    bool mixedMode = false;         // 连发优先：Hold按住时压制Toggle（默认关闭）
    std::vector<KeyDescriptor> pauseKeys;  // 暂停按键列表：按住时暂停所有开关模式连发
    std::vector<KeyBinding> bindings;
    std::vector<MacroConfig> macros;
};

// ── 全局配置 ───────────────────────────────────
struct AppConfig {
    // 全局设置
    bool beepEnabled = true;
    CloseAction closeAction = CloseAction::Ask;
    std::string faction = "none";   // 门派主题ID

    // 自定义提示音路径（空表示使用默认内置音频）
    std::wstring customSoundOn;     // 按键开启提示音路径
    std::wstring customSoundOff;    // 按键关闭提示音路径
    int soundVolume = 80;           // 按键提示音音量（0-100）

    bool macroBeepEnabled = true;   // 宏循环是否开启启停提示音
    std::wstring macroSoundOn;      // 宏开启提示音路径
    std::wstring macroSoundOff;     // 宏关闭提示音路径
    int macroSoundVolume = 80;      // 宏提示音音量（0-100）

    // 配置方案管理
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
