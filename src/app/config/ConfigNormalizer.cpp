#include "config/ConfigNormalizer.h"

#include <algorithm>

void ConfigNormalizer::normalize(AppConfig& config, std::uint64_t& activeMacroId) {
    for (auto& profile : config.profiles) {
        for (auto& binding : profile.bindings) {
            if (binding.id == 0) binding.id = nextBindingId(profile);
        }
        for (auto& macro : profile.macros) {
            if (macro.id == 0) macro.id = nextMacroId(profile);
        }
        if (profile.macros.empty()) {
            MacroConfig defaultMacro;
            defaultMacro.id = 1;
            defaultMacro.name = L"\u9ed8\u8ba4\u5b8f";
            defaultMacro.enabled = true;
            profile.macros.push_back(defaultMacro);
        }
    }

    auto profileIt = std::find_if(config.profiles.begin(), config.profiles.end(),
        [&](const ProfileConfig& profile) { return profile.id == config.activeProfileId; });
    if (profileIt == config.profiles.end() && !config.profiles.empty()) {
        profileIt = config.profiles.begin();
    }
    if (profileIt == config.profiles.end() || profileIt->macros.empty()) return;

    const bool macroExists = std::any_of(profileIt->macros.begin(), profileIt->macros.end(),
        [&](const MacroConfig& macro) { return macro.id == activeMacroId; });
    if (activeMacroId == 0 || !macroExists) {
        activeMacroId = profileIt->macros.front().id;
    }
}

std::uint64_t ConfigNormalizer::nextBindingId(const ProfileConfig& profile) {
    std::uint64_t maxId = 0;
    for (const auto& binding : profile.bindings) {
        maxId = std::max(maxId, binding.id);
    }
    return maxId + 1;
}

std::uint64_t ConfigNormalizer::nextMacroId(const ProfileConfig& profile) {
    std::uint64_t maxId = 0;
    for (const auto& macro : profile.macros) {
        maxId = std::max(maxId, macro.id);
    }
    return maxId + 1;
}
