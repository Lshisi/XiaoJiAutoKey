#include "ConfigStore.h"

#include "JsonLite.h"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <system_error>
#include <Windows.h>

namespace {

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
        binding.delayMs = std::max(*delay, kMinDelayMs);
        const auto mode = GetStringField(*entry, "mode").value_or("toggle");
        if (mode == "hold") binding.mode = RepeatMode::Hold;
        else if (mode == "independent") binding.mode = RepeatMode::Independent;
        else if (mode == "sequence") binding.mode = RepeatMode::Sequence;
        else binding.mode = RepeatMode::Toggle;
        binding.suppressMixedMode = GetBoolField(*entry, "suppressMixedMode").value_or(false);
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
                        if (auto keyText = GetStringField(*stepObj, "key")) {
                            auto k = StorageToKeyDescriptor(ToWide(*keyText));
                            if (k.has_value()) step.key = *k;
                        }
                        step.delayAfterMs = std::max(
                            GetIntField(*stepObj, "delayAfterMs").value_or(kDefaultDelayMs),
                            kMinDelayMs);
                        if (step.key.IsValid()) macro.steps.push_back(step);
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
        const char* mode = "toggle";
        if (b.mode == RepeatMode::Hold) mode = "hold";
        else if (b.mode == RepeatMode::Independent) mode = "independent";
        else if (b.mode == RepeatMode::Sequence) mode = "sequence";
        entry.emplace("mode", std::string(mode));
        entry.emplace("delayMs", b.delayMs);
        entry.emplace("suppressMixedMode", b.suppressMixedMode);
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
            stepObj.emplace("key", ToUtf8(KeyDescriptorToStorage(step.key)));
            stepObj.emplace("delayAfterMs", step.delayAfterMs);
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
    AppConfig config;

    if (!std::filesystem::exists(configPath_)) {
        // First run: create the default profile and macro.
        ProfileConfig def;
        def.id = 1;
        def.name = L"\u9ed8\u8ba4\u914d\u7f6e";
        MacroConfig defaultMacro;
        defaultMacro.id = 1;
        defaultMacro.name = L"\u9ed8\u8ba4\u914d\u7f6e";
        defaultMacro.enabled = true;
        config.profiles.push_back(def);
        config.activeProfileId = 1;
        config.macros.push_back(defaultMacro);
        config.activeMacroId = 1;
        return config;
    }

    std::ifstream file(configPath_, std::ios::binary);
    if (!file) return config;

    std::stringstream buffer;
    buffer << file.rdbuf();

    jsonlite::Value root;
    std::string error;
    if (!jsonlite::Parse(buffer.str(), root, error)) return config;

    const auto* object = root.AsObject();
    if (!object) return config;

    // Global settings
    if (const auto backgroundAutoRun = GetBoolField(*object, "backgroundAutoRun"); backgroundAutoRun.has_value())
        config.backgroundAutoRun = *backgroundAutoRun;
    if (const auto beep = GetBoolField(*object, "beepEnabled"); beep.has_value())
        config.beepEnabled = *beep;
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

    if (const auto faction = GetStringField(*object, "faction"); faction.has_value() && !faction->empty())
        config.faction = *faction;
    if (config.faction.empty() || config.faction == "none")
        config.faction = "cangjian";

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

bool ConfigStore::Save(const AppConfig& config, std::wstring& error) const {
    jsonlite::Object root;

    // Global settings
    root.emplace("backgroundAutoRun", config.backgroundAutoRun);
    root.emplace("beepEnabled", config.beepEnabled);
    root.emplace("statusOverlayShowRepeat", config.statusOverlayShowRepeat);
    root.emplace("statusOverlayShowMacro", config.statusOverlayShowMacro);
    root.emplace("statusOverlayLocked", config.statusOverlayLocked);
    root.emplace("statusOverlayPositionSaved", config.statusOverlayPositionSaved);
    root.emplace("statusOverlayX", config.statusOverlayX);
    root.emplace("statusOverlayY", config.statusOverlayY);
    root.emplace("statusOverlayOpacity", std::clamp(config.statusOverlayOpacity, 30, 100));

    std::string closeActionStr = "ask";
    if (config.closeAction == CloseAction::Minimize) closeActionStr = "minimize";
    else if (config.closeAction == CloseAction::Exit) closeActionStr = "exit";
    root.emplace("closeAction", closeActionStr);
    const char* driverMode = "legacy";
    if (config.driverMode == DriverMode::Hid) driverMode = "hid";
    else if (config.driverMode == DriverMode::Usb) driverMode = "usb";
    root.emplace("driverMode", std::string(driverMode));

    if (!config.faction.empty())
        root.emplace("faction", config.faction);

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

    return WriteAtomically(content, error);
}

bool ConfigStore::SaveRawValidated(const std::string& content, std::wstring& error) const {
    if (!IsValidConfigText(content)) {
        error = L"\u914d\u7f6e\u5185\u5bb9\u683c\u5f0f\u65e0\u6548";
        return false;
    }
    return WriteAtomically(!content.empty() && content.back() == '\n' ? content : content + "\n", error);
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

bool ConfigStore::WriteAtomically(const std::string& content, std::wstring& error) const {
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

    std::ofstream file(tempPath, std::ios::binary | std::ios::trunc);
    if (!file) {
        error = L"\u65e0\u6cd5\u5199\u5165\u914d\u7f6e\u4e34\u65f6\u6587\u4ef6";
        return false;
    }
    file.write(content.data(), static_cast<std::streamsize>(content.size()));
    file.flush();
    if (!file.good()) {
        error = L"\u5199\u5165\u914d\u7f6e\u6587\u4ef6\u5931\u8d25";
        file.close();
        std::filesystem::remove(tempPath, ec);
        return false;
    }
    file.close();

    if (std::filesystem::exists(configPath_)) {
        std::filesystem::copy_file(
            configPath_, backupPath, std::filesystem::copy_options::overwrite_existing, ec);
        ec.clear();
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
