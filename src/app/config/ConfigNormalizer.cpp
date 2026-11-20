#include "MacroActions.h"
#include "config/ConfigNormalizer.h"

#include <algorithm>

void ConfigNormalizer::normalize(AppConfig& config) {
    for (auto& profile : config.profiles) {
        profile.defaultDelayMs = std::max(profile.defaultDelayMs, kMinDelayMs);
        for (auto& binding : profile.bindings) {
            if (binding.id == 0) binding.id = nextBindingId(profile);
            binding.delayMs = std::max(binding.delayMs, kMinDelayMs);
            if (binding.mode != RepeatMode::Hold) binding.suppressMixedMode = false;
            if (!CanUseClickPosition(binding.key)) binding.clickPos = {};
        }
    }

    std::uint64_t nextMacro = nextMacroId(config);
    for (auto& macro : config.macros) {
        if (macro.id == 0) macro.id = nextMacro++;
        std::vector<std::uint64_t> stepIds;
        stepIds.reserve(macro.steps.size());
        for (const auto& step : macro.steps) {
            if (step.id != 0) stepIds.push_back(step.id);
        }
        std::sort(stepIds.begin(), stepIds.end());
        const bool hasDuplicateStepIds = std::adjacent_find(stepIds.begin(), stepIds.end()) != stepIds.end();
        if (stepIds.size() != macro.steps.size() || hasDuplicateStepIds) {
            std::uint64_t nextStep = nextStepId(macro);
            for (auto& step : macro.steps) step.id = nextStep++;
        }
        for (auto& step : macro.steps) {
            NormalizeMacroStep(step);
        }
    }
    if (config.macros.empty()) {
        MacroConfig defaultMacro;
        defaultMacro.id = 1;
        defaultMacro.name = L"\u9ed8\u8ba4\u914d\u7f6e";
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

std::uint64_t ConfigNormalizer::nextStepId(const MacroConfig& macro) {
    std::uint64_t maxId = 0;
    for (const auto& step : macro.steps) maxId = std::max(maxId, step.id);
    return maxId + 1;
}

std::uint64_t ConfigNormalizer::nextMacroId(const AppConfig& config) {
    std::uint64_t maxId = 0;
    for (const auto& macro : config.macros) {
        maxId = std::max(maxId, macro.id);
    }
    return maxId + 1;
}
