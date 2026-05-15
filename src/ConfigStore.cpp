#include "ConfigStore.h"

#include "JsonLite.h"

#include <fstream>
#include <sstream>

namespace {

std::optional<std::string> GetStringField(const jsonlite::Object& object, const std::string& key) {
    const auto it = object.find(key);
    if (it == object.end()) {
        return std::nullopt;
    }
    if (const auto* value = it->second.AsString()) {
        return *value;
    }
    return std::nullopt;
}

std::optional<bool> GetBoolField(const jsonlite::Object& object, const std::string& key) {
    const auto it = object.find(key);
    if (it == object.end()) {
        return std::nullopt;
    }
    if (const auto* value = it->second.AsBool()) {
        return *value;
    }
    return std::nullopt;
}

std::optional<int> GetIntField(const jsonlite::Object& object, const std::string& key) {
    const auto it = object.find(key);
    if (it == object.end()) {
        return std::nullopt;
    }
    if (const auto* value = it->second.AsNumber()) {
        return static_cast<int>(*value);
    }
    return std::nullopt;
}

std::optional<std::uint64_t> GetUInt64Field(const jsonlite::Object& object, const std::string& key) {
    const auto it = object.find(key);
    if (it == object.end()) {
        return std::nullopt;
    }
    if (const auto* value = it->second.AsNumber()) {
        return static_cast<std::uint64_t>(*value);
    }
    return std::nullopt;
}

}  // namespace

ConfigStore::ConfigStore(std::filesystem::path configPath) : configPath_(std::move(configPath)) {}

AppConfig ConfigStore::Load() const {
    AppConfig config;

    if (!std::filesystem::exists(configPath_)) {
        return config;
    }

    std::ifstream file(configPath_, std::ios::binary);
    if (!file) {
        return config;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();

    jsonlite::Value root;
    std::string error;
    if (!jsonlite::Parse(buffer.str(), root, error)) {
        return config;
    }

    const auto* object = root.AsObject();
    if (object == nullptr) {
        return config;
    }

    if (const auto defaultDelay = GetIntField(*object, "defaultDelayMs"); defaultDelay.has_value() && *defaultDelay > 0) {
        config.defaultDelayMs = *defaultDelay;
    }

    if (const auto beepEnabled = GetBoolField(*object, "beepEnabled"); beepEnabled.has_value()) {
        config.beepEnabled = *beepEnabled;
    }

    if (const auto mixedMode = GetBoolField(*object, "mixedMode"); mixedMode.has_value()) {
        config.mixedMode = *mixedMode;
    }

    if (const auto closeActionStr = GetStringField(*object, "closeAction"); closeActionStr.has_value()) {
        if (*closeActionStr == "minimize") {
            config.closeAction = CloseAction::Minimize;
        } else if (*closeActionStr == "exit") {
            config.closeAction = CloseAction::Exit;
        } else {
            config.closeAction = CloseAction::Ask;
        }
    }

    if (const auto customPath = GetStringField(*object, "customConfigPath"); customPath.has_value() && !customPath->empty()) {
        config.customConfigPath = ToWide(*customPath);
    }

    if (const auto faction = GetStringField(*object, "faction"); faction.has_value() && !faction->empty()) {
        config.faction = *faction;
    }

    if (const auto hotkeyText = GetStringField(*object, "controlHotkey"); hotkeyText.has_value()) {
        config.controlHotkey = StorageToKeyDescriptor(ToWide(*hotkeyText));
    }

    const auto bindingsIt = object->find("bindings");
    if (bindingsIt != object->end()) {
        const auto* bindings = bindingsIt->second.AsArray();
        if (bindings != nullptr) {
            for (const auto& item : *bindings) {
                const auto* entry = item.AsObject();
                if (entry == nullptr) {
                    continue;
                }

                const auto keyText = GetStringField(*entry, "key");
                const auto key = keyText.has_value() ? StorageToKeyDescriptor(ToWide(*keyText)) : std::nullopt;
                const auto delay = GetIntField(*entry, "delayMs");
                if (!key.has_value() || !delay.has_value() || *delay <= 0) {
                    continue;
                }

                KeyBinding binding;
                binding.id = GetUInt64Field(*entry, "id").value_or(0);
                binding.enabled = GetBoolField(*entry, "enabled").value_or(true);
                binding.key = *key;
                binding.delayMs = *delay;

                const auto mode = GetStringField(*entry, "mode").value_or("toggle");
                binding.mode = (mode == "hold") ? RepeatMode::Hold : RepeatMode::Toggle;
                config.bindings.push_back(binding);
            }
        }
    }

    // Load macros
    const auto macrosIt = object->find("macros");
    if (macrosIt != object->end()) {
        const auto* macros = macrosIt->second.AsArray();
        if (macros != nullptr) {
            for (const auto& item : *macros) {
                const auto* entry = item.AsObject();
                if (entry == nullptr) continue;

                MacroConfig macro;
                macro.id = GetUInt64Field(*entry, "id").value_or(0);
                macro.enabled = GetBoolField(*entry, "enabled").value_or(true);

                if (auto name = GetStringField(*entry, "name")) {
                    macro.name = ToWide(*name);
                }

                auto modeStr = GetStringField(*entry, "triggerMode").value_or("toggle");
                macro.triggerMode = (modeStr == "hold") ? MacroTriggerMode::Hold : MacroTriggerMode::Toggle;

                if (auto triggerText = GetStringField(*entry, "triggerKey")) {
                    macro.triggerKey = StorageToKeyDescriptor(ToWide(*triggerText));
                }

                macro.loopCount = GetIntField(*entry, "loopCount").value_or(0);

                // Load steps
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
                                step.delayAfterMs = GetIntField(*stepObj, "delayAfterMs").value_or(50);
                                if (step.key.IsValid()) {
                                    macro.steps.push_back(step);
                                }
                            }
                        }
                    }
                }

                if (!macro.name.empty()) {
                    config.macros.push_back(std::move(macro));
                }
            }
        }
    }

    return config;
}

bool ConfigStore::Save(const AppConfig& config, std::wstring& error) const {
    jsonlite::Object root;
    root.emplace("controlHotkey", ToUtf8(KeyDescriptorToStorage(config.controlHotkey)));
    root.emplace("defaultDelayMs", config.defaultDelayMs);
    root.emplace("beepEnabled", config.beepEnabled);
    root.emplace("mixedMode", config.mixedMode);

    std::string closeActionStr = "ask";
    if (config.closeAction == CloseAction::Minimize) closeActionStr = "minimize";
    else if (config.closeAction == CloseAction::Exit) closeActionStr = "exit";
    root.emplace("closeAction", closeActionStr);

    if (!config.customConfigPath.empty()) {
        root.emplace("customConfigPath", ToUtf8(config.customConfigPath));
    }

    if (!config.faction.empty() && config.faction != "none") {
        root.emplace("faction", config.faction);
    }

    jsonlite::Array bindings;
    bindings.reserve(config.bindings.size());
    for (const auto& binding : config.bindings) {
        jsonlite::Object entry;
        entry.emplace("id", static_cast<double>(binding.id));
        entry.emplace("enabled", binding.enabled);
        entry.emplace("key", ToUtf8(KeyDescriptorToStorage(binding.key)));
        entry.emplace("mode", std::string(binding.mode == RepeatMode::Hold ? "hold" : "toggle"));
        entry.emplace("delayMs", binding.delayMs);
        bindings.emplace_back(std::move(entry));
    }

    root.emplace("bindings", std::move(bindings));

    // Save macros
    jsonlite::Array macros;
    macros.reserve(config.macros.size());
    for (const auto& macro : config.macros) {
        jsonlite::Object entry;
        entry.emplace("id", static_cast<double>(macro.id));
        entry.emplace("enabled", macro.enabled);
        entry.emplace("name", ToUtf8(macro.name));
        entry.emplace("triggerMode", std::string(macro.triggerMode == MacroTriggerMode::Hold ? "hold" : "toggle"));
        entry.emplace("triggerKey", ToUtf8(KeyDescriptorToStorage(macro.triggerKey)));
        entry.emplace("loopCount", macro.loopCount);

        jsonlite::Array steps;
        for (const auto& step : macro.steps) {
            jsonlite::Object stepObj;
            stepObj.emplace("key", ToUtf8(KeyDescriptorToStorage(step.key)));
            stepObj.emplace("delayAfterMs", step.delayAfterMs);
            steps.emplace_back(std::move(stepObj));
        }
        entry.emplace("steps", std::move(steps));
        macros.emplace_back(std::move(entry));
    }
    root.emplace("macros", std::move(macros));

    const std::string content = jsonlite::Stringify(root) + "\n";

    std::ofstream file(configPath_, std::ios::binary | std::ios::trunc);
    if (!file) {
        error = L"无法写入配置文件";
        return false;
    }

    file.write(content.data(), static_cast<std::streamsize>(content.size()));
    if (!file.good()) {
        error = L"写入配置文件失败";
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
