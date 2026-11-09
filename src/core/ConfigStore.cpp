#include "MacroActions.h"
#include "ConfigStore.h"

#include "JsonLite.h"

#include <QDate>
#include <algorithm>
#include <cmath>
#include <limits>
#include <fstream>
#include <sstream>
#include <system_error>
#include <Windows.h>

namespace {

class ConfigWriteLock {
public:
    explicit ConfigWriteLock(std::uint32_t timeoutMs)
        : mutex_(CreateMutexW(nullptr, FALSE, L"XiaoJiAutoKey_ConfigWrite_Mutex")) {
        if (!mutex_) return;
        const DWORD result = WaitForSingleObject(mutex_, timeoutMs);
        locked_ = result == WAIT_OBJECT_0 || result == WAIT_ABANDONED;
    }

    ~ConfigWriteLock() {
        if (locked_) ReleaseMutex(mutex_);
        if (mutex_) CloseHandle(mutex_);
    }

    [[nodiscard]] bool locked() const noexcept { return locked_; }

private:
    HANDLE mutex_ = nullptr;
    bool locked_ = false;
};

AppConfig DefaultConfig(bool offerOnboarding = false) {
    AppConfig config;
    config.onboardingStatus = offerOnboarding ? OnboardingStatus::Pending : OnboardingStatus::Completed;
    config.onboardingSeenTips = offerOnboarding ? 0 : 7;
    ProfileConfig profile;
    profile.id = 1;
    profile.name = L"\u9ed8\u8ba4\u914d\u7f6e";
    MacroConfig macro;
    macro.id = 1;
    macro.name = L"\u9ed8\u8ba4\u914d\u7f6e";
    macro.enabled = true;
    config.profiles.push_back(std::move(profile));
    config.activeProfileId = 1;
    config.macros.push_back(std::move(macro));
    config.activeMacroId = 1;
    return config;
}

bool ReadFileContent(const std::filesystem::path& path, std::string& content) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return false;
    std::ostringstream stream;
    stream << file.rdbuf();
    if (!file.good() && !file.eof()) return false;
    content = stream.str();
    return true;
}

bool WriteFileDurably(const std::filesystem::path& path, const std::string& content) {
    HANDLE file = ::CreateFileW(
        path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;

    bool success = true;
    std::size_t offset = 0;
    while (offset < content.size()) {
        const std::size_t remaining = content.size() - offset;
        const DWORD chunkSize = static_cast<DWORD>(std::min<std::size_t>(remaining, MAXDWORD));
        DWORD written = 0;
        if (!::WriteFile(file, content.data() + offset, chunkSize, &written, nullptr) || written == 0) {
            success = false;
            break;
        }
        offset += written;
    }
    if (success) success = ::FlushFileBuffers(file) != FALSE;
    if (!::CloseHandle(file)) success = false;
    return success;
}

void HideFileIfPresent(const std::filesystem::path& path) noexcept {
    const DWORD attributes = ::GetFileAttributesW(path.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_HIDDEN) != 0) {
        return;
    }
    ::SetFileAttributesW(path.c_str(), attributes | FILE_ATTRIBUTE_HIDDEN);
}

bool IsNewerFile(
    const std::filesystem::path& candidate, const std::filesystem::path& reference) {
    std::error_code candidateError;
    std::error_code referenceError;
    const auto candidateTime = std::filesystem::last_write_time(candidate, candidateError);
    const auto referenceTime = std::filesystem::last_write_time(reference, referenceError);
    return !candidateError && (referenceError || candidateTime >= referenceTime);
}

std::optional<std::string> GetStringField(const jsonlite::Object& object, const std::string& key) {
    const auto it = object.find(key);
    if (it == object.end()) return std::nullopt;
    if (const auto* value = it->second.AsString()) return *value;
    return std::nullopt;
}

std::optional<bool> GetBoolField(const jsonlite::Object& object, const std::string& key) {
    const auto it = object.find(key);
    if (it == object.end()) return std::nullopt;
    if (const auto* value = it->second.AsBool()) return *value;
    return std::nullopt;
}

std::optional<int> GetIntField(const jsonlite::Object& object, const std::string& key) {
    const auto it = object.find(key);
    if (it == object.end()) return std::nullopt;
    if (const auto* value = it->second.AsNumber()) return static_cast<int>(*value);
    return std::nullopt;
}

std::optional<std::uint64_t> GetUInt64Field(const jsonlite::Object& object, const std::string& key) {
    const auto it = object.find(key);
    if (it == object.end()) return std::nullopt;
    if (const auto* value = it->second.AsNumber()) return static_cast<std::uint64_t>(*value);
    return std::nullopt;
}

std::optional<double> GetDoubleField(const jsonlite::Object& object, const std::string& key) {
    const auto it = object.find(key);
    if (it == object.end()) return std::nullopt;
    if (const auto* value = it->second.AsNumber()) return *value;
    return std::nullopt;
}

std::vector<KeyBinding> LoadBindings(const jsonlite::Object& object) {
    std::vector<KeyBinding> result;
    const auto bindingsIt = object.find("bindings");
    if (bindingsIt == object.end()) return result;
    const auto* bindings = bindingsIt->second.AsArray();
    if (!bindings) return result;

    for (const auto& item : *bindings) {
        const auto* entry = item.AsObject();
        if (!entry) continue;

        const auto keyText = GetStringField(*entry, "key");
        const auto key = keyText.has_value() ? StorageToKeyDescriptor(ToWide(*keyText)) : std::nullopt;
        const auto delay = GetIntField(*entry, "delayMs");
        if (!key.has_value() || !delay.has_value() || *delay <= 0) continue;

        KeyBinding binding;
        binding.id = GetUInt64Field(*entry, "id").value_or(0);
        binding.enabled = GetBoolField(*entry, "enabled").value_or(true);
        binding.key = *key;
        if (auto name = GetStringField(*entry, "name")) binding.name = ToWide(*name);
        binding.delayMs = std::max(*delay, kMinDelayMs);
        const auto mode = GetStringField(*entry, "mode").value_or("toggle");
        if (mode == "hold") binding.mode = RepeatMode::Hold;
        else if (mode == "independent") binding.mode = RepeatMode::Independent;
        else if (mode == "sequence") binding.mode = RepeatMode::Sequence;
        else if (mode == "longPress") binding.mode = RepeatMode::LongPress;
        else binding.mode = RepeatMode::Toggle;
        binding.suppressMixedMode = GetBoolField(*entry, "suppressMixedMode").value_or(false);
        if (const auto clickIt = entry->find("clickPos"); clickIt != entry->end()) {
            if (const auto* clickObj = clickIt->second.AsObject()) {
                if (GetStringField(*clickObj, "mode").value_or("") == "fixed") {
                    binding.clickPos.fixedPosition = true;
                    if (const auto x = GetDoubleField(*clickObj, "x")) binding.clickPos.anchorX = *x;
                    if (const auto y = GetDoubleField(*clickObj, "y")) binding.clickPos.anchorY = *y;
                    binding.clickPos.anchorX = std::clamp(binding.clickPos.anchorX, 0.0, 1.0);
                    binding.clickPos.anchorY = std::clamp(binding.clickPos.anchorY, 0.0, 1.0);
                }
            }
        }
        result.push_back(binding);
    }
    return result;
}

std::vector<MacroConfig> LoadMacros(const jsonlite::Object& object) {
    std::vector<MacroConfig> result;
    const auto macrosIt = object.find("macros");
    if (macrosIt == object.end()) return result;
    const auto* macros = macrosIt->second.AsArray();
    if (!macros) return result;

    for (const auto& item : *macros) {
        const auto* entry = item.AsObject();
        if (!entry) continue;

        MacroConfig macro;
        macro.id = GetUInt64Field(*entry, "id").value_or(0);
        macro.enabled = GetBoolField(*entry, "enabled").value_or(true);
        if (auto name = GetStringField(*entry, "name")) macro.name = ToWide(*name);
        auto modeStr = GetStringField(*entry, "triggerMode").value_or("toggle");
        macro.triggerMode = (modeStr == "hold") ? MacroTriggerMode::Hold : MacroTriggerMode::Toggle;
        if (auto triggerText = GetStringField(*entry, "triggerKey"))
            macro.triggerKey = StorageToKeyDescriptor(ToWide(*triggerText));
        macro.loopCount = GetIntField(*entry, "loopCount").value_or(0);
        const auto pauseKeysIt = entry->find("pauseKeys");
        if (pauseKeysIt != entry->end()) {
            if (const auto* pauseArr = pauseKeysIt->second.AsArray()) {
                for (const auto& pkItem : *pauseArr) {
                    if (const auto* pkStr = pkItem.AsString()) {
                        auto pk = StorageToKeyDescriptor(ToWide(*pkStr));
                        if (pk.has_value()) macro.pauseKeys.push_back(*pk);
                    }
                }
            }
        }

        auto stepsIt = entry->find("steps");
        if (stepsIt != entry->end()) {
            if (auto* stepsArr = stepsIt->second.AsArray()) {
                for (const auto& stepItem : *stepsArr) {
                    if (auto* stepObj = stepItem.AsObject()) {
                        MacroStep step;
                        const auto action = ParseMacroAction(GetStringField(*stepObj, "action").value_or("click"));
                        if (!action) continue;
                        step.action = *action;
                        step.durationMs = GetIntField(*stepObj, "durationMs").value_or(500);
                        step.wheelDelta = GetIntField(*stepObj, "wheelDelta").value_or(120);
                        step.id = GetUInt64Field(*stepObj, "id").value_or(0);
                        step.enabled = GetBoolField(*stepObj, "enabled").value_or(true);
                        if (auto name = GetStringField(*stepObj, "name")) step.name = ToWide(*name);
                        if (auto keyText = GetStringField(*stepObj, "key")) {
                            auto k = StorageToKeyDescriptor(ToWide(*keyText));
                            if (k.has_value()) step.key = *k;
                        }
                        step.delayAfterMs = std::max(
                            GetIntField(*stepObj, "delayAfterMs").value_or(kDefaultDelayMs),
                            0);
                        if (const auto clickIt = stepObj->find("clickPos"); clickIt != stepObj->end()) {
                            if (const auto* clickObj = clickIt->second.AsObject()) {
                                if (GetStringField(*clickObj, "mode").value_or("") == "fixed") {
                                    step.clickPos.fixedPosition = true;
                                    if (const auto x = GetDoubleField(*clickObj, "x")) step.clickPos.anchorX = *x;
                                    if (const auto y = GetDoubleField(*clickObj, "y")) step.clickPos.anchorY = *y;
                                    step.clickPos.anchorX = std::clamp(step.clickPos.anchorX, 0.0, 1.0);
                                    step.clickPos.anchorY = std::clamp(step.clickPos.anchorY, 0.0, 1.0);
                                }
                            }
                        }
                        NormalizeMacroStep(step);
                        if (!MacroActionNeedsKey(step.action) || step.key.IsValid()) macro.steps.push_back(step);
                    }
                }
            }
        }
        if (!macro.name.empty()) result.push_back(std::move(macro));
    }
    return result;
}

jsonlite::Array SerializeBindings(const std::vector<KeyBinding>& bindings) {
    jsonlite::Array arr;
    arr.reserve(bindings.size());
    for (const auto& b : bindings) {
        jsonlite::Object entry;
        entry.emplace("id", static_cast<double>(b.id));
        entry.emplace("enabled", b.enabled);
        entry.emplace("key", ToUtf8(KeyDescriptorToStorage(b.key)));
        if (!b.name.empty()) entry.emplace("name", ToUtf8(b.name));
        const char* mode = "toggle";
        if (b.mode == RepeatMode::Hold) mode = "hold";
        else if (b.mode == RepeatMode::Independent) mode = "independent";
        else if (b.mode == RepeatMode::Sequence) mode = "sequence";
        else if (b.mode == RepeatMode::LongPress) mode = "longPress";
        entry.emplace("mode", std::string(mode));
        entry.emplace("delayMs", b.delayMs);
        entry.emplace("suppressMixedMode", b.suppressMixedMode);
        if (b.key.device == InputDevice::Mouse && b.clickPos.IsFixed()) {
            jsonlite::Object clickObj;
            clickObj.emplace("mode", std::string("fixed"));
            clickObj.emplace("x", std::clamp(b.clickPos.anchorX, 0.0, 1.0));
            clickObj.emplace("y", std::clamp(b.clickPos.anchorY, 0.0, 1.0));
            entry.emplace("clickPos", std::move(clickObj));
        }
        arr.emplace_back(std::move(entry));
    }
    return arr;
}

jsonlite::Array SerializeMacros(const std::vector<MacroConfig>& macros) {
    jsonlite::Array arr;
    arr.reserve(macros.size());
    for (const auto& macro : macros) {
        jsonlite::Object entry;
        entry.emplace("id", static_cast<double>(macro.id));
        entry.emplace("enabled", macro.enabled);
        entry.emplace("name", ToUtf8(macro.name));
        entry.emplace("triggerMode", std::string(macro.triggerMode == MacroTriggerMode::Hold ? "hold" : "toggle"));
        entry.emplace("triggerKey", ToUtf8(KeyDescriptorToStorage(macro.triggerKey)));
        entry.emplace("loopCount", macro.loopCount);
        if (!macro.pauseKeys.empty()) {
            jsonlite::Array pauseKeys;
            pauseKeys.reserve(macro.pauseKeys.size());
            for (const auto& key : macro.pauseKeys) {
                pauseKeys.emplace_back(ToUtf8(KeyDescriptorToStorage(key)));
            }
            entry.emplace("pauseKeys", std::move(pauseKeys));
        }

        jsonlite::Array steps;
        for (const auto& step : macro.steps) {
            jsonlite::Object stepObj;
            stepObj.emplace("id", static_cast<double>(step.id));
            stepObj.emplace("enabled", step.enabled);
            stepObj.emplace("action", MacroActionStorage(step.action));
            stepObj.emplace("durationMs", step.durationMs);
            stepObj.emplace("wheelDelta", step.wheelDelta);
            if (MacroActionNeedsKey(step.action)) stepObj.emplace("key", ToUtf8(KeyDescriptorToStorage(step.key)));
            if (!step.name.empty()) stepObj.emplace("name", ToUtf8(step.name));
            stepObj.emplace("delayAfterMs", step.delayAfterMs);
            if (step.key.device == InputDevice::Mouse && step.clickPos.IsFixed()) {
                jsonlite::Object clickObj;
                clickObj.emplace("mode", std::string("fixed"));
                clickObj.emplace("x", std::clamp(step.clickPos.anchorX, 0.0, 1.0));
                clickObj.emplace("y", std::clamp(step.clickPos.anchorY, 0.0, 1.0));
                stepObj.emplace("clickPos", std::move(clickObj));
            }
            steps.emplace_back(std::move(stepObj));
        }
        entry.emplace("steps", std::move(steps));
        arr.emplace_back(std::move(entry));
    }
    return arr;
}

}  // namespace

ConfigStore::ConfigStore(std::filesystem::path configPath) : configPath_(std::move(configPath)) {}

AppConfig ConfigStore::Load() const {
    auto sourcePath = configPath_;
    auto tempPath = configPath_;
    tempPath += L".tmp";
    auto backupPath = configPath_;
    backupPath += L".bak";
    HideFileIfPresent(backupPath);
    const bool currentValid = IsValidConfigFile(sourcePath);
    const bool tempValid = IsValidConfigFile(tempPath);
    if (tempValid && (!currentValid || IsNewerFile(tempPath, sourcePath))) {
        sourcePath = std::move(tempPath);
    } else if (!currentValid && IsValidConfigFile(backupPath)) {
        sourcePath = std::move(backupPath);
    } else if (!currentValid) {
        std::error_code ec;
        const bool configMissing = !std::filesystem::exists(configPath_, ec) && !ec;
        const bool tempMissing = !std::filesystem::exists(tempPath, ec) && !ec;
        const bool backupMissing = !std::filesystem::exists(backupPath, ec) && !ec;
        return DefaultConfig(configMissing && tempMissing && backupMissing);
    }

    AppConfig config;
    std::ifstream file(sourcePath, std::ios::binary);
    if (!file) return DefaultConfig();

    std::stringstream buffer;
    buffer << file.rdbuf();

    jsonlite::Value root;
    std::string error;
    if (!jsonlite::Parse(buffer.str(), root, error)) return DefaultConfig();

    const auto* object = root.AsObject();
    if (!object) return DefaultConfig();

    // Older configurations stay out of the automatic tour, including restored backups.
    config.onboardingSeenTips = 7;
    config.onboardingSeenActions = 0;
    const auto onboardingIt = object->find("onboarding");
    if (onboardingIt != object->end()) {
        if (const auto* onboarding = onboardingIt->second.AsObject()) {
            config.onboardingVersion = std::max(1, GetIntField(*onboarding, "version").value_or(1));
            const auto status = GetStringField(*onboarding, "status").value_or("completed");
            if (status == "pending") config.onboardingStatus = OnboardingStatus::Pending;
            else if (status == "inProgress") config.onboardingStatus = OnboardingStatus::InProgress;
            else if (status == "skipped") config.onboardingStatus = OnboardingStatus::Skipped;
            config.onboardingStep = std::clamp(GetIntField(*onboarding, "step").value_or(0), 0, 5);
            config.onboardingMacroStep = std::clamp(GetIntField(*onboarding, "macroStep").value_or(0), 0, 2);
            config.onboardingSeenTips = GetIntField(*onboarding, "seenTips").value_or(0) & 63;
            config.onboardingSeenActions = GetIntField(*onboarding, "seenActions").value_or(0) & 63;
        }
    }

    // Global settings
    if (const auto it = object->find("dailyUsageMs"); it != object->end()) {
        if (const auto* days = it->second.AsObject()) {
            for (const auto& [date, value] : *days) {
                const QString key = QString::fromStdString(date);
                const QDate day = QDate::fromString(key, Qt::ISODate);
                const auto* milliseconds = value.AsNumber();
                if (day.isValid() && day.year() > 0 && day.toString(Qt::ISODate) == key
                    && milliseconds && std::isfinite(*milliseconds)
                    && *milliseconds > 0
                    && *milliseconds < static_cast<double>(std::numeric_limits<std::int64_t>::max())) {
                    config.dailyUsageMs.emplace(date, static_cast<std::int64_t>(*milliseconds));
                }
            }
        }
    }
    if (const auto backgroundAutoRun = GetBoolField(*object, "backgroundAutoRun"); backgroundAutoRun.has_value())
        config.backgroundAutoRun = *backgroundAutoRun;
    if (const auto beep = GetBoolField(*object, "beepEnabled"); beep.has_value())
        config.beepEnabled = *beep;
    if (const auto gameGate = GetBoolField(*object, "gameGateEnabled"); gameGate.has_value())
        config.gameGateEnabled = *gameGate;
    if (const auto autoStart = GetBoolField(*object, "autoStartEnabled"); autoStart.has_value())
        config.autoStartEnabled = *autoStart;
    {
        const auto processesIt = object->find("gameGateProcesses");
        if (processesIt != object->end()) {
            if (const auto* processes = processesIt->second.AsArray()) {
                for (const auto& item : *processes) {
                    if (const auto* name = item.AsString()) {
                        if (!name->empty() && name->size() <= 260) {
                            config.gameGateProcesses.push_back(*name);
                        }
                    }
                }
            }
        }
    }
    if (const auto showRepeat = GetBoolField(*object, "statusOverlayShowRepeat"); showRepeat.has_value())
        config.statusOverlayShowRepeat = *showRepeat;
    if (const auto showMacro = GetBoolField(*object, "statusOverlayShowMacro"); showMacro.has_value())
        config.statusOverlayShowMacro = *showMacro;
    if (const auto locked = GetBoolField(*object, "statusOverlayLocked"); locked.has_value())
        config.statusOverlayLocked = *locked;
    if (const auto overlayPos = GetBoolField(*object, "statusOverlayPositionSaved"); overlayPos.has_value())
        config.statusOverlayPositionSaved = *overlayPos;
    if (const auto overlayX = GetIntField(*object, "statusOverlayX"); overlayX.has_value())
        config.statusOverlayX = *overlayX;
    if (const auto overlayY = GetIntField(*object, "statusOverlayY"); overlayY.has_value())
        config.statusOverlayY = *overlayY;
    if (const auto opacity = GetIntField(*object, "statusOverlayOpacity"); opacity.has_value())
        config.statusOverlayOpacity = std::clamp(*opacity, 30, 100);
    if (const auto blur = GetBoolField(*object, "statusOverlayNativeBlur"); blur.has_value())
        config.statusOverlayNativeBlur = *blur;
    if (const auto value = GetBoolField(*object, "repeatOverlayShowHotkey"); value.has_value())
        config.repeatOverlay.showHotkey = *value;
    if (const auto value = GetBoolField(*object, "repeatOverlayShowIndependent"); value.has_value())
        config.repeatOverlay.showIndependent = *value;
    if (const auto value = GetBoolField(*object, "repeatOverlayShowHold"); value.has_value())
        config.repeatOverlay.showHold = *value;
    if (const auto value = GetBoolField(*object, "repeatOverlayShowToggle"); value.has_value())
        config.repeatOverlay.showToggle = *value;
    if (const auto value = GetBoolField(*object, "repeatOverlayShowLongPress"); value.has_value())
        config.repeatOverlay.showLongPress = *value;
    if (const auto value = GetBoolField(*object, "repeatOverlayShowSequence"); value.has_value())
        config.repeatOverlay.showSequence = *value;
    if (const auto value = GetBoolField(*object, "repeatOverlayShowSequenceHotkey"); value.has_value())
        config.repeatOverlay.showSequenceHotkey = *value;
    if (const auto value = GetBoolField(*object, "repeatOverlayShowPauseKeys"); value.has_value())
        config.repeatOverlay.showPauseKeys = *value;
    if (const auto value = GetBoolField(*object, "macroOverlayShowHotkey"); value.has_value())
        config.macroOverlay.showHotkey = *value;
    if (const auto value = GetBoolField(*object, "macroOverlayShowTriggerMode"); value.has_value())
        config.macroOverlay.showTriggerMode = *value;
    if (const auto value = GetBoolField(*object, "macroOverlayShowLoopCount"); value.has_value())
        config.macroOverlay.showLoopCount = *value;
    if (const auto value = GetBoolField(*object, "macroOverlayShowPauseKeys"); value.has_value())
        config.macroOverlay.showPauseKeys = *value;
    if (const auto value = GetBoolField(*object, "macroOverlayShowCurrentStep"); value.has_value())
        config.macroOverlay.showCurrentStep = *value;

    if (const auto startupStr = GetStringField(*object, "startupAction"); startupStr.has_value()) {
        config.startupAction = *startupStr == "tray"
            ? StartupAction::Tray
            : StartupAction::Window;
    }

    if (const auto closeStr = GetStringField(*object, "closeAction"); closeStr.has_value()) {
        if (*closeStr == "minimize") config.closeAction = CloseAction::Minimize;
        else if (*closeStr == "exit") config.closeAction = CloseAction::Exit;
        else config.closeAction = CloseAction::Ask;
    }

    if (const auto driverMode = GetStringField(*object, "driverMode"); driverMode.has_value()) {
        if (*driverMode == "hid") config.driverMode = DriverMode::Hid;
        else if (*driverMode == "usb" || *driverMode == "pico") config.driverMode = DriverMode::Usb;
        else config.driverMode = DriverMode::Legacy;
    }
    if (const auto uninstalled = GetBoolField(*object, "ddDriverUninstalledByUser"); uninstalled.has_value())
        config.ddDriverUninstalledByUser = *uninstalled;
    if (const auto uninstalled = GetBoolField(*object, "hidDriverUninstalledByUser"); uninstalled.has_value())
        config.hidDriverUninstalledByUser = *uninstalled;

    if (const auto faction = GetStringField(*object, "faction"); faction.has_value() && !faction->empty())
        config.faction = *faction;
    if (config.faction.empty() || config.faction == "none")
        config.faction = "cangjian";

    if (const auto latestKnownVersion = GetStringField(*object, "latestKnownVersion");
        latestKnownVersion.has_value() && !latestKnownVersion->empty())
        config.latestKnownVersion = *latestKnownVersion;

    if (const auto soundOn = GetStringField(*object, "customSoundOn"); soundOn.has_value() && !soundOn->empty())
        config.customSoundOn = ToWide(*soundOn);
    if (const auto soundOff = GetStringField(*object, "customSoundOff"); soundOff.has_value() && !soundOff->empty())
        config.customSoundOff = ToWide(*soundOff);
    if (const auto volume = GetIntField(*object, "soundVolume"); volume.has_value())
        config.soundVolume = std::clamp(*volume, 0, 100);
    if (const auto macroBeep = GetBoolField(*object, "macroBeepEnabled"); macroBeep.has_value())
        config.macroBeepEnabled = *macroBeep;
    if (const auto soundOn = GetStringField(*object, "macroSoundOn"); soundOn.has_value() && !soundOn->empty())
        config.macroSoundOn = ToWide(*soundOn);
    if (const auto soundOff = GetStringField(*object, "macroSoundOff"); soundOff.has_value() && !soundOff->empty())
        config.macroSoundOff = ToWide(*soundOff);
    if (const auto volume = GetIntField(*object, "macroSoundVolume"); volume.has_value())
        config.macroSoundVolume = std::clamp(*volume, 0, 100);

    config.activeProfileId = GetUInt64Field(*object, "activeProfileId").value_or(0);
    config.activeMacroId = GetUInt64Field(*object, "activeMacroId").value_or(0);
    config.macros = LoadMacros(*object);

    // Load profiles
    const auto profilesIt = object->find("profiles");
    if (profilesIt != object->end()) {
        const auto* profilesArr = profilesIt->second.AsArray();
        if (profilesArr) {
            for (const auto& pItem : *profilesArr) {
                const auto* pObj = pItem.AsObject();
                if (!pObj) continue;

                ProfileConfig profile;
                profile.id = GetUInt64Field(*pObj, "id").value_or(0);
                if (auto name = GetStringField(*pObj, "name")) profile.name = ToWide(*name);
                profile.enabled = GetBoolField(*pObj, "enabled").value_or(true);
                if (auto hotkeyText = GetStringField(*pObj, "controlHotkey"))
                    profile.controlHotkey = StorageToKeyDescriptor(ToWide(*hotkeyText));
                if (auto switchKeyText = GetStringField(*pObj, "sequenceSwitchKey"))
                    profile.sequenceSwitchKey = StorageToKeyDescriptor(ToWide(*switchKeyText));
                if (auto delay = GetIntField(*pObj, "defaultDelayMs"); delay.has_value() && *delay > 0)
                    profile.defaultDelayMs = std::max(*delay, kMinDelayMs);
                if (auto mixed = GetBoolField(*pObj, "mixedMode"); mixed.has_value())
                    profile.mixedMode = *mixed;
                if (auto wasdProtection = GetBoolField(*pObj, "wasdProtection"); wasdProtection.has_value())
                    profile.wasdProtection = *wasdProtection;
                profile.rememberSequencePosition = GetBoolField(*pObj, "rememberSequencePosition").value_or(true);

                // Load pause keys
                const auto pauseKeysIt = pObj->find("pauseKeys");
                if (pauseKeysIt != pObj->end()) {
                    if (const auto* pauseArr = pauseKeysIt->second.AsArray()) {
                        for (const auto& pkItem : *pauseArr) {
                            if (const auto* pkStr = pkItem.AsString()) {
                                auto pk = StorageToKeyDescriptor(ToWide(*pkStr));
                                if (pk.has_value()) profile.pauseKeys.push_back(*pk);
                            }
                        }
                    }
                }

                profile.bindings = LoadBindings(*pObj);

                if (profile.id > 0 && !profile.name.empty())
                    config.profiles.push_back(std::move(profile));
            }
        }
    }

    // If no profiles loaded, create a default one
    if (config.profiles.empty()) {
        ProfileConfig def;
        def.id = 1;
        def.name = L"\u9ed8\u8ba4\u914d\u7f6e";
        MacroConfig defaultMacro;
        defaultMacro.id = 1;
        defaultMacro.name = L"\u9ed8\u8ba4\u914d\u7f6e";
        defaultMacro.enabled = true;
        config.profiles.push_back(def);
        config.activeProfileId = 1;
    }

    if (config.macros.empty()) {
        MacroConfig defaultMacro;
        defaultMacro.id = 1;
        defaultMacro.name = L"\u9ed8\u8ba4\u914d\u7f6e";
        defaultMacro.enabled = true;
        config.macros.push_back(defaultMacro);
        config.activeMacroId = 1;
    }

    // Validate activeProfileId
    bool found = false;
    for (const auto& p : config.profiles) {
        if (p.id == config.activeProfileId) { found = true; break; }
    }
    if (!found) config.activeProfileId = config.profiles.front().id;

    return config;
}

bool ConfigStore::Save(
    const AppConfig& config, std::wstring& error, std::uint32_t lockTimeoutMs) const {
    jsonlite::Object root;

    const char* onboardingStatus = "completed";
    if (config.onboardingStatus == OnboardingStatus::Pending) onboardingStatus = "pending";
    else if (config.onboardingStatus == OnboardingStatus::InProgress) onboardingStatus = "inProgress";
    else if (config.onboardingStatus == OnboardingStatus::Skipped) onboardingStatus = "skipped";
    jsonlite::Object onboarding;
    onboarding.emplace("version", config.onboardingVersion);
    onboarding.emplace("status", std::string(onboardingStatus));
    onboarding.emplace("step", std::clamp(config.onboardingStep, 0, 5));
    onboarding.emplace("macroStep", std::clamp(config.onboardingMacroStep, 0, 2));
    onboarding.emplace("seenTips", config.onboardingSeenTips & 63);
    onboarding.emplace("seenActions", config.onboardingSeenActions & 63);
    root.emplace("onboarding", std::move(onboarding));

    jsonlite::Object usage;
    for (const auto& [date, milliseconds] : config.dailyUsageMs) {
        usage.emplace(date, static_cast<double>(milliseconds));
    }
    root.emplace("dailyUsageMs", std::move(usage));

    // Global settings
    root.emplace("backgroundAutoRun", config.backgroundAutoRun);
    root.emplace("beepEnabled", config.beepEnabled);
    root.emplace("gameGateEnabled", config.gameGateEnabled);
    root.emplace("autoStartEnabled", config.autoStartEnabled);
    if (!config.gameGateProcesses.empty()) {
        jsonlite::Array processes;
        for (const auto& name : config.gameGateProcesses) {
            processes.emplace_back(name);
        }
        root.emplace("gameGateProcesses", std::move(processes));
    }
    root.emplace("statusOverlayShowRepeat", config.statusOverlayShowRepeat);
    root.emplace("statusOverlayShowMacro", config.statusOverlayShowMacro);
    root.emplace("statusOverlayLocked", config.statusOverlayLocked);
    root.emplace("statusOverlayPositionSaved", config.statusOverlayPositionSaved);
    root.emplace("statusOverlayX", config.statusOverlayX);
    root.emplace("statusOverlayY", config.statusOverlayY);
    root.emplace("statusOverlayOpacity", std::clamp(config.statusOverlayOpacity, 30, 100));
    root.emplace("statusOverlayNativeBlur", config.statusOverlayNativeBlur);
    root.emplace("repeatOverlayShowHotkey", config.repeatOverlay.showHotkey);
    root.emplace("repeatOverlayShowIndependent", config.repeatOverlay.showIndependent);
    root.emplace("repeatOverlayShowHold", config.repeatOverlay.showHold);
    root.emplace("repeatOverlayShowToggle", config.repeatOverlay.showToggle);
    root.emplace("repeatOverlayShowLongPress", config.repeatOverlay.showLongPress);
    root.emplace("repeatOverlayShowSequence", config.repeatOverlay.showSequence);
    root.emplace("repeatOverlayShowSequenceHotkey", config.repeatOverlay.showSequenceHotkey);
    root.emplace("repeatOverlayShowPauseKeys", config.repeatOverlay.showPauseKeys);
    root.emplace("macroOverlayShowHotkey", config.macroOverlay.showHotkey);
    root.emplace("macroOverlayShowTriggerMode", config.macroOverlay.showTriggerMode);
    root.emplace("macroOverlayShowLoopCount", config.macroOverlay.showLoopCount);
    root.emplace("macroOverlayShowPauseKeys", config.macroOverlay.showPauseKeys);
    root.emplace("macroOverlayShowCurrentStep", config.macroOverlay.showCurrentStep);

    root.emplace("startupAction",
        std::string(config.startupAction == StartupAction::Window ? "window" : "tray"));

    std::string closeActionStr = "ask";
    if (config.closeAction == CloseAction::Minimize) closeActionStr = "minimize";
    else if (config.closeAction == CloseAction::Exit) closeActionStr = "exit";
    root.emplace("closeAction", closeActionStr);
    const char* driverMode = "legacy";
    if (config.driverMode == DriverMode::Hid) driverMode = "hid";
    else if (config.driverMode == DriverMode::Usb) driverMode = "usb";
    root.emplace("driverMode", std::string(driverMode));
    root.emplace("ddDriverUninstalledByUser", config.ddDriverUninstalledByUser);
    root.emplace("hidDriverUninstalledByUser", config.hidDriverUninstalledByUser);

    if (!config.faction.empty())
        root.emplace("faction", config.faction);
    if (!config.latestKnownVersion.empty())
        root.emplace("latestKnownVersion", config.latestKnownVersion);

    if (!config.customSoundOn.empty())
        root.emplace("customSoundOn", ToUtf8(config.customSoundOn));
    if (!config.customSoundOff.empty())
        root.emplace("customSoundOff", ToUtf8(config.customSoundOff));
    root.emplace("soundVolume", std::clamp(config.soundVolume, 0, 100));
    root.emplace("macroBeepEnabled", config.macroBeepEnabled);
    if (!config.macroSoundOn.empty())
        root.emplace("macroSoundOn", ToUtf8(config.macroSoundOn));
    if (!config.macroSoundOff.empty())
        root.emplace("macroSoundOff", ToUtf8(config.macroSoundOff));
    root.emplace("macroSoundVolume", std::clamp(config.macroSoundVolume, 0, 100));

    root.emplace("activeProfileId", static_cast<double>(config.activeProfileId));
    root.emplace("activeMacroId", static_cast<double>(config.activeMacroId));
    root.emplace("macros", SerializeMacros(config.macros));

    // Profiles
    jsonlite::Array profilesArr;
    profilesArr.reserve(config.profiles.size());
    for (const auto& profile : config.profiles) {
        jsonlite::Object pObj;
        pObj.emplace("id", static_cast<double>(profile.id));
        pObj.emplace("name", ToUtf8(profile.name));
        pObj.emplace("enabled", profile.enabled);
        pObj.emplace("controlHotkey", ToUtf8(KeyDescriptorToStorage(profile.controlHotkey)));
        pObj.emplace("sequenceSwitchKey", ToUtf8(KeyDescriptorToStorage(profile.sequenceSwitchKey)));
        pObj.emplace("defaultDelayMs", profile.defaultDelayMs);
        pObj.emplace("mixedMode", profile.mixedMode);
        pObj.emplace("wasdProtection", profile.wasdProtection);
        pObj.emplace("rememberSequencePosition", profile.rememberSequencePosition);

        // Serialize pause keys
        jsonlite::Array pauseKeysArr;
        for (const auto& pk : profile.pauseKeys) {
            pauseKeysArr.emplace_back(ToUtf8(KeyDescriptorToStorage(pk)));
        }
        if (!pauseKeysArr.empty())
            pObj.emplace("pauseKeys", std::move(pauseKeysArr));

        pObj.emplace("bindings", SerializeBindings(profile.bindings));
        profilesArr.emplace_back(std::move(pObj));
    }
    root.emplace("profiles", std::move(profilesArr));

    const std::string content = jsonlite::Stringify(root) + "\n";

    return WriteAtomically(content, error, lockTimeoutMs);
}

bool ConfigStore::RestoreFrom(const std::filesystem::path& sourcePath, std::wstring& error) const {
    std::string content;
    if (!ReadFileContent(sourcePath, content) || !IsValidConfigText(content)) {
        error = L"\u914d\u7f6e\u6062\u590d\u6587\u4ef6\u65e0\u6548";
        return false;
    }
    if (content.empty() || content.back() != '\n') content.push_back('\n');
    return WriteAtomically(content, error);
}

bool ConfigStore::IsValidConfigText(const std::string& content) {
    jsonlite::Value root;
    std::string parseError;
    return jsonlite::Parse(content, root, parseError) && root.AsObject() != nullptr;
}

bool ConfigStore::IsValidConfigFile(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return false;
    std::ostringstream content;
    content << file.rdbuf();
    return IsValidConfigText(content.str());
}

bool ConfigStore::WriteAtomically(
    const std::string& content, std::wstring& error, std::uint32_t lockTimeoutMs) const {
    ConfigWriteLock writeLock(lockTimeoutMs);
    if (!writeLock.locked()) {
        error = L"\u65e0\u6cd5\u83b7\u53d6\u914d\u7f6e\u5199\u5165\u9501";
        return false;
    }

    std::error_code ec;
    const auto parent = configPath_.parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent, ec);
        if (ec) {
            error = L"\u65e0\u6cd5\u521b\u5efa\u914d\u7f6e\u76ee\u5f55";
            return false;
        }
    }

    auto tempPath = configPath_;
    tempPath += L".tmp";
    auto backupPath = configPath_;
    backupPath += L".bak";
    auto backupTempPath = backupPath;
    backupTempPath += L".tmp";

    if (!IsValidConfigText(content)) {
        error = L"\u914d\u7f6e\u5185\u5bb9\u683c\u5f0f\u65e0\u6548";
        return false;
    }
    if (!WriteFileDurably(tempPath, content)) {
        error = L"\u65e0\u6cd5\u5199\u5165\u914d\u7f6e\u4e34\u65f6\u6587\u4ef6";
        std::filesystem::remove(tempPath, ec);
        return false;
    }
    if (!IsValidConfigFile(tempPath)) {
        error = L"\u914d\u7f6e\u4e34\u65f6\u6587\u4ef6\u6821\u9a8c\u5931\u8d25";
        std::filesystem::remove(tempPath, ec);
        return false;
    }

    if (IsValidConfigFile(configPath_)) {
        std::string previousContent;
        if (!ReadFileContent(configPath_, previousContent) ||
            !WriteFileDurably(backupTempPath, previousContent) ||
            !IsValidConfigFile(backupTempPath) ||
            !::MoveFileExW(backupTempPath.c_str(), backupPath.c_str(),
                MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
            error = L"\u65e0\u6cd5\u521b\u5efa\u914d\u7f6e\u5907\u4efd";
            std::filesystem::remove(backupTempPath, ec);
            std::filesystem::remove(tempPath, ec);
            return false;
        }
        HideFileIfPresent(backupPath);
    }

    if (!::MoveFileExW(
            tempPath.c_str(), configPath_.c_str(),
            MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        error = L"\u65e0\u6cd5\u539f\u5b50\u66ff\u6362\u914d\u7f6e\u6587\u4ef6";
        std::filesystem::remove(tempPath, ec);
        return false;
    }
    return true;
}

const std::filesystem::path& ConfigStore::GetPath() const noexcept {
    return configPath_;
}

void ConfigStore::SetPath(std::filesystem::path newPath) {
    configPath_ = std::move(newPath);
}
