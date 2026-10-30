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
    LongPress,
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

// 鼠标键的点击位置：跟随鼠标，或锚定在游戏窗口客户区内的固定比例点位。
struct ClickPosition {
    bool fixedPosition = false;
    double anchorX = 0.5;  // 0.0-1.0，相对游戏窗口客户区
    double anchorY = 0.5;

    [[nodiscard]] bool IsFixed() const noexcept { return fixedPosition; }
};

struct KeyBinding {
    std::uint64_t id = 0;
    bool enabled = true;
    KeyDescriptor key;
    RepeatMode mode = RepeatMode::Toggle;
    int delayMs = kDefaultDelayMs;
    bool suppressMixedMode = false;
    ClickPosition clickPos;  // 仅对鼠标键有意义
};

enum class CloseAction {
    Ask,
    Minimize,
    Exit,
};

enum class StartupAction {
    Tray,
    Window,
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
    ClickPosition clickPos;  // 仅鼠标键使用
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
    bool rememberSequencePosition = true;
    std::optional<KeyDescriptor> sequenceSwitchKey;
    std::vector<KeyDescriptor> pauseKeys;
    std::vector<KeyBinding> bindings;
};

struct RepeatOverlayOptions {
    bool showHotkey = true;
    bool showIndependent = false;
    bool showHold = false;
    bool showToggle = false;
    bool showSequence = false;
    bool showSequenceHotkey = false;
    bool showPauseKeys = false;
    bool showLongPress = false;
};

struct MacroOverlayOptions {
    bool showHotkey = true;
    bool showTriggerMode = false;
    bool showLoopCount = true;
    bool showPauseKeys = false;
};

struct AppConfig {
    bool backgroundAutoRun = false;
    bool beepEnabled = true;
    // 仅当游戏客户端在前台时生效（离开游戏自动暂停并释放按键）
    bool gameGateEnabled = false;
    // 额外识别的游戏进程名（小写，支持 "name*" 前缀通配）
    std::vector<std::string> gameGateProcesses;
    bool statusOverlayShowRepeat = false;
    bool statusOverlayShowMacro = false;
    bool statusOverlayLocked = false;
    bool statusOverlayPositionSaved = false;
    int statusOverlayX = 6;
    int statusOverlayY = 6;
    int statusOverlayOpacity = 70;
    bool statusOverlayNativeBlur = true;
    RepeatOverlayOptions repeatOverlay;
    MacroOverlayOptions macroOverlay;
    // 开机自启：登录 Windows 后通过 HKCU Run 注册表键自动运行小叽。
    bool autoStartEnabled = false;
    StartupAction startupAction = StartupAction::Window;
    CloseAction closeAction = CloseAction::Ask;
    DriverMode driverMode = DriverMode::Legacy;
    bool ddDriverUninstalledByUser = false;
    bool hidDriverUninstalledByUser = false;
    std::string faction = "cangjian";
    // Latest version seen through a manual update check; only used to keep
    // the update badge visible across restarts. Never triggers a check.
    std::string latestKnownVersion;

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

    [[nodiscard]] bool IsDriverUninstalledByUser(DriverMode mode) const noexcept {
        return mode == DriverMode::Legacy ? ddDriverUninstalledByUser
            : mode == DriverMode::Hid && hidDriverUninstalledByUser;
    }
    void SetDriverUninstalledByUser(DriverMode mode, bool value) noexcept {
        if (mode == DriverMode::Legacy) ddDriverUninstalledByUser = value;
        else if (mode == DriverMode::Hid) hidDriverUninstalledByUser = value;
    }
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
