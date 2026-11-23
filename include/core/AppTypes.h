#pragma once

#include "SoundSettings.h"

#include <cstdint>
#include <cstddef>
#include <map>
#include <optional>
#include <string>
#include <vector>

#ifndef XIAOJI_PLUGIN_CATALOG_URL
#define XIAOJI_PLUGIN_CATALOG_URL ""
#endif
#ifndef XIAOJI_PLUGIN_CATALOG_PUBLIC_KEY
#define XIAOJI_PLUGIN_CATALOG_PUBLIC_KEY ""
#endif
#ifndef XIAOJI_PLUGIN_CATALOG_FALLBACK_URL
#define XIAOJI_PLUGIN_CATALOG_FALLBACK_URL ""
#endif

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
constexpr std::size_t kMaxMacroSteps = 512;

constexpr std::uint8_t kKeyModifierCtrl = 0x01;
constexpr std::uint8_t kKeyModifierShift = 0x02;
constexpr std::uint8_t kKeyModifierAlt = 0x04;

struct KeyDescriptor {
    InputDevice device = InputDevice::Keyboard;
    std::uint32_t code = 0;
    // 组合键的必需修饰键；物理事件中表示事件发生时的修饰键快照。
    std::uint8_t modifiers = 0;

    [[nodiscard]] bool IsValid() const noexcept {
        return code != 0;
    }

    [[nodiscard]] bool operator==(const KeyDescriptor& other) const noexcept = default;
};

[[nodiscard]] inline bool CanUseClickPosition(const KeyDescriptor& key) noexcept {
    // 左右键的虚拟键值也是跨平台配置格式中的键值。
    return key.device == InputDevice::Mouse && (key.code == 0x01 || key.code == 0x02);
}

// 鼠标左、右键的点击位置：跟随鼠标，或锚定在游戏窗口客户区内的固定比例点位。
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
    std::wstring name;
    RepeatMode mode = RepeatMode::Toggle;
    int delayMs = kDefaultDelayMs;
    bool suppressMixedMode = false;
    ClickPosition clickPos;  // 仅对鼠标左、右键有意义
    bool martialAssist = false;
    bool preventAccidentalStop = false;
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

enum class OnboardingStatus {
    Pending,
    InProgress,
    Completed,
    Skipped,
};

enum class DriverMode {
    Xiaoji,
    Legacy,
    Usb,
};

// Retired HID is an uninstall target only; it cannot be selected or loaded.
enum class DriverRemovalTarget { Xiaoji, Legacy, RetiredHid };

enum class MacroTriggerMode {
    Toggle,
    Hold,
};

enum class MacroAction { Click, Hold, Down, Up, Wait, Wheel };

struct MacroStep {
    std::uint64_t id = 0;
    bool enabled = true;
    KeyDescriptor key;
    std::wstring name;
    MacroAction action = MacroAction::Click;
    int durationMs = 500;
    int wheelDelta = 120;
    int delayAfterMs = kDefaultDelayMs;
    ClickPosition clickPos;  // 仅鼠标左、右键使用
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
    bool showCurrentStep = true;
};

struct PluginPreferences {
    bool enabled = true;
    bool removed = false;
    std::string activeVersion;
    std::string pendingVersion;
    std::string rollbackVersion;
    std::string trialVersion;
    std::vector<std::string> verifiedActivations;
    bool activationInProgress = false;
    bool backgroundEnabled = false;
    bool removalPendingRestart = false;
    std::string removalError;
    std::string settingsJson = "{}";
    std::string windowStateJson = "{}";
};

struct AppConfig {
    int onboardingVersion = 1;
    OnboardingStatus onboardingStatus = OnboardingStatus::Completed;
    int onboardingStep = 0;
    int onboardingMacroStep = 0;
    int onboardingSeenTips = 0;
    int onboardingSeenActions = 0;
    bool backgroundAutoRun = false;
    bool beepEnabled = true;
    bool macroBeepEnabled = true;
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
    DriverMode driverMode = DriverMode::Xiaoji;
    std::string activeDriverId = "xiaoji.driver.xiaoji/default";
    std::map<std::string, PluginPreferences> plugins;
    std::vector<std::string> sidebarPageOrder;
    bool automaticUpdateChecks = true;
    std::string pluginRegistryUrl = XIAOJI_PLUGIN_CATALOG_URL;
    std::string pluginRegistryFallbackUrl = XIAOJI_PLUGIN_CATALOG_FALLBACK_URL;
    std::string pluginRegistryPublicKey = XIAOJI_PLUGIN_CATALOG_PUBLIC_KEY;
    bool xiaojiDriverUninstalledByUser = false;
    bool ddDriverUninstalledByUser = false;
    std::string faction = "cangjian";
    // Cached update result. The update coordinator schedules network checks.
    std::string latestKnownVersion;
    std::map<std::string, std::int64_t> dailyUsageMs;

    SoundSettings sounds = DefaultSoundSettings();

    std::uint64_t activeProfileId = 0;
    std::vector<ProfileConfig> profiles;
    std::uint64_t activeMacroId = 0;
    std::vector<MacroConfig> macros;

    [[nodiscard]] bool IsDriverUninstalledByUser(DriverMode mode) const noexcept {
        return mode == DriverMode::Legacy ? ddDriverUninstalledByUser
            : mode == DriverMode::Xiaoji && xiaojiDriverUninstalledByUser;
    }
    void SetDriverUninstalledByUser(DriverMode mode, bool value) noexcept {
        if (mode == DriverMode::Legacy) ddDriverUninstalledByUser = value;
        else if (mode == DriverMode::Xiaoji) xiaojiDriverUninstalledByUser = value;
    }
};

std::wstring ToWide(const std::string& text);
std::string ToUtf8(const std::wstring& text);

std::wstring GetKeyDisplayName(const KeyDescriptor& key);
[[nodiscard]] bool IsKeyboardModifierKey(const KeyDescriptor& key) noexcept;
[[nodiscard]] inline std::uint8_t ModifierMaskFromKey(const KeyDescriptor& key) noexcept {
    if (key.device != InputDevice::Keyboard) return 0;
    // Virtual-key values also form the platform-neutral configuration format.
    switch (key.code) {
    case 0x11: case 0xA2: case 0xA3: return kKeyModifierCtrl;
    case 0x10: case 0xA0: case 0xA1: return kKeyModifierShift;
    case 0x12: case 0xA4: case 0xA5: return kKeyModifierAlt;
    default: return 0;
    }
}
[[nodiscard]] std::wstring ModifierPrefix(std::uint8_t modifiers);
[[nodiscard]] bool SamePrimaryKey(const KeyDescriptor& left, const KeyDescriptor& right) noexcept;
[[nodiscard]] bool KeyMatchesEvent(const KeyDescriptor& binding, const KeyDescriptor& event) noexcept;
[[nodiscard]] bool KeyReleasedByEvent(const KeyDescriptor& binding, const KeyDescriptor& event) noexcept;
[[nodiscard]] std::vector<KeyDescriptor> ExpandKeyComponents(const KeyDescriptor& key);
[[nodiscard]] bool IsReservedKeyCombination(const KeyDescriptor& key) noexcept;
std::wstring RepeatModeToText(RepeatMode mode);
bool IsToggleLikeRepeatMode(RepeatMode mode) noexcept;
std::wstring DeviceToText(InputDevice device);

constexpr std::uint32_t kNumpadEnterKeyCode = 0x0100000D;

KeyDescriptor NormalizeKeyDescriptor(const KeyDescriptor& key);
KeyDescriptor KeyboardHookKeyDescriptor(std::uint32_t vkCode, std::uint32_t scanCode, std::uint32_t flags);

std::wstring KeyDescriptorToStorage(const KeyDescriptor& key);
std::wstring KeyDescriptorToStorage(const std::optional<KeyDescriptor>& key);
std::optional<KeyDescriptor> StorageToKeyDescriptor(const std::wstring& text);
