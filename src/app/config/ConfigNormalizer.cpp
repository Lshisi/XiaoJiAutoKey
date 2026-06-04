#include "config/ConfigNormalizer.h"

#include <algorithm>

void ConfigNormalizer::normalize(AppConfig& config) {
    for (auto& profile : config.profiles) {
        for (auto& binding : profile.bindings) {
            if (binding.id == 0) binding.id = nextBindingId(profile);
        }
    }

    std::uint64_t nextMacro = nextMacroId(config);
    for (auto& macro : config.macros) {
        if (macro.id == 0) macro.id = nextMacro++;
    }
    if (config.macros.empty()) {
        MacroConfig defaultMacro;
        defaultMacro.id = 1;
        defaultMacro.name = L"\u9ed8\u8ba4\u5b8f";
        defaultMacro.enabled = true;
        config.macros.push_back(defaultMacro);
    }

    const bool macroExists = std::any_of(config.macros.begin(), config.macros.end(),
        [&](const MacroConfig& macro) { return macro.id == config.activeMacroId; });
    if (config.activeMacroId == 0 || !macroExists) {
        config.activeMacroId = config.macros.front().id;
    }
}

std::uint64_t ConfigNormalizer::nextBindingId(const ProfileConfig& profile) {
    std::uint64_t maxId = 0;
    for (const auto& binding : profile.bindings) {
        maxId = std::max(maxId, binding.id);
    }
    return maxId + 1;
}

std::uint64_t ConfigNormalizer::nextMacroId(const AppConfig& config) {
    std::uint64_t maxId = 0;
    for (const auto& macro : config.macros) {
        maxId = std::max(maxId, macro.id);
    }
    return maxId + 1;
}
