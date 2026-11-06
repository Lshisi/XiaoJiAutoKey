#include "MacroActions.h"
#include "controllers/MacroController.h"

#include "config/ActiveConfigValidator.h"

#include <Windows.h>
#include <algorithm>
#include <iterator>

namespace {

ControllerResult validatePauseKeyAgainstSteps(const KeyDescriptor& key, const MacroConfig& macro) {
    for (const auto& step : macro.steps) {
        if (!step.enabled) continue;
        auto validation = ActiveConfigValidator::validateModifierOverlap(
            key, L"暂停键", step.key, L"循环步骤");
        if (!validation.ok) return validation;
    }
    return ControllerResult::success();
}

ControllerResult validateKeyAgainstPauseKeys(const KeyDescriptor& key, const MacroConfig& macro) {
    for (const auto& pauseKey : macro.pauseKeys) {
        auto validation = ActiveConfigValidator::validateModifierOverlap(
            pauseKey, L"暂停键", key, L"循环步骤");
        if (!validation.ok) return validation;
    }
    return ControllerResult::success();
}

}

MacroController::MacroController(AppConfig& config) : config_(config) {}

MacroConfig* MacroController::activeMacro() {
    for (auto& macro : config_.macros) {
        if (macro.id == config_.activeMacroId) return &macro;
    }
    return nullptr;
}

const MacroConfig* MacroController::activeMacro() const {
    for (const auto& macro : config_.macros) {
        if (macro.id == config_.activeMacroId) return &macro;
    }
    return nullptr;
}

MacroConfig* MacroController::macroById(std::uint64_t macroId) {
    for (auto& macro : config_.macros) {
        if (macro.id == macroId) return &macro;
    }
    return nullptr;
}

const MacroConfig* MacroController::macroById(std::uint64_t macroId) const {

    for (const auto& macro : config_.macros) {
        if (macro.id == macroId) return &macro;
    }
    return nullptr;
}

std::uint64_t MacroController::nextMacroId() const {
    return nextMacroId(config_);
}

std::uint64_t MacroController::nextMacroId(const AppConfig& config) {
    std::uint64_t maxId = 0;
    for (const auto& macro : config.macros) maxId = std::max(maxId, macro.id);
    return maxId + 1;
}

std::uint64_t MacroController::nextStepId(const MacroConfig& macro) {
    std::uint64_t maxId = 0;
    for (const auto& step : macro.steps) maxId = std::max(maxId, step.id);
    return maxId + 1;
}

void MacroController::resetActiveMacro() {
    if (config_.macros.empty()) {
        MacroConfig macro;
        macro.id = 1;
        macro.name = L"\u9ed8\u8ba4\u914d\u7f6e";
        macro.enabled = true;
        config_.macros.push_back(macro);
    }

    if (config_.activeMacroId == 0 || std::none_of(config_.macros.begin(), config_.macros.end(),
        [this](const MacroConfig& macro) { return macro.id == config_.activeMacroId; })) {
        config_.activeMacroId = config_.macros.front().id;
    }
}

void MacroController::ensureActiveMacro() {
    resetActiveMacro();
}

ControllerResult MacroController::switchMacro(std::uint64_t macroId) {
    if (!macroById(macroId)) return ControllerResult::failure(QString::fromWCharArray(L"\u5b8f\u4e0d\u5b58\u5728\u3002"));

    config_.activeMacroId = macroId;
    return ControllerResult::success();
}

ControllerResult MacroController::addMacro(const QString& name, std::uint64_t& createdMacroId) {
    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty()) return ControllerResult::failure(QString::fromWCharArray(L"\u5b8f\u540d\u79f0\u4e0d\u80fd\u4e3a\u7a7a\u3002"));

    MacroConfig macro;
    macro.id = nextMacroId(config_);
    macro.name = trimmed.toStdWString();
    macro.enabled = true;
    createdMacroId = macro.id;
    config_.macros.push_back(std::move(macro));
    config_.activeMacroId = createdMacroId;
    return ControllerResult::success();
}

ControllerResult MacroController::duplicateActiveMacro(const QString& name, std::uint64_t& createdMacroId) {
    const auto* source = activeMacro();
    if (!source) return ControllerResult::failure(QString::fromWCharArray(L"\u5b8f\u4e0d\u5b58\u5728\u3002"));

    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty()) return ControllerResult::failure(QString::fromWCharArray(L"\u5b8f\u540d\u79f0\u4e0d\u80fd\u4e3a\u7a7a\u3002"));

    MacroConfig macro = *source;
    macro.id = nextMacroId(config_);
    macro.name = trimmed.toStdWString();
    std::uint64_t stepId = 0;
    for (auto& step : macro.steps) step.id = ++stepId;
    createdMacroId = macro.id;

    const auto sourcePosition = std::find_if(config_.macros.begin(), config_.macros.end(),
        [source](const MacroConfig& item) { return item.id == source->id; });
    config_.macros.insert(sourcePosition + 1, std::move(macro));
    config_.activeMacroId = createdMacroId;
    return ControllerResult::success();
}

ControllerResult MacroController::renameActiveMacro(const QString& name) {
    auto* macro = activeMacro();
    if (!macro) return ControllerResult::failure(QString::fromWCharArray(L"\u5b8f\u4e0d\u5b58\u5728\u3002"));

    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty()) return ControllerResult::failure(QString::fromWCharArray(L"\u5b8f\u540d\u79f0\u4e0d\u80fd\u4e3a\u7a7a\u3002"));

    macro->name = trimmed.toStdWString();
    return ControllerResult::success();
}

ControllerResult MacroController::deleteActiveMacro(std::uint64_t& newActiveMacroId) {
    if (config_.macros.size() <= 1) {
        return ControllerResult::failure(QString::fromWCharArray(L"\u81f3\u5c11\u9700\u8981\u4fdd\u7559\u4e00\u4e2a\u5b8f\u914d\u7f6e\u3002"));
    }

    const auto* macro = activeMacro();
    if (!macro) return ControllerResult::failure(QString::fromWCharArray(L"\u5b8f\u4e0d\u5b58\u5728\u3002"));

    const std::uint64_t deletedId = macro->id;
    config_.macros.erase(
        std::remove_if(config_.macros.begin(), config_.macros.end(),
            [deletedId](const MacroConfig& item) { return item.id == deletedId; }),
        config_.macros.end());

    newActiveMacroId = config_.macros.empty() ? 0 : config_.macros.front().id;
    config_.activeMacroId = newActiveMacroId;
    return ControllerResult::success();
}

ControllerResult MacroController::setActiveMacroEnabled(bool enabled) {
    auto* macro = activeMacro();
    if (!macro) return ControllerResult::failure(QString::fromWCharArray(L"\u5b8f\u4e0d\u5b58\u5728\u3002"));

    if (enabled) {
        MacroConfig candidate = *macro;
        candidate.enabled = true;
        auto validation = ActiveConfigValidator::validateMacroModifierOverlaps(&candidate);
        if (!validation.ok) return validation;
    }

    macro->enabled = enabled;
    return ControllerResult::success();
}

ControllerResult MacroController::validateStepKey(const MacroConfig& macro, const KeyDescriptor& key) const {
    if (auto validation = ActiveConfigValidator::validateKeyCombination(key); !validation.ok) return validation;
    if (auto validation = validateKeyAgainstPauseKeys(key, macro); !validation.ok) return validation;
    if (macro.triggerKey.has_value() && *macro.triggerKey == key) {
        return ControllerResult::failure(QString::fromWCharArray(L"\u8be5\u6309\u952e\u5df2\u88ab\u8bbe\u7f6e\u4e3a\u5f53\u524d\u5b8f\u7684\u542f\u505c\u5feb\u6377\u952e\u3002"));
    }
    for (const auto& pauseKey : macro.pauseKeys) {
        if (pauseKey == key) {
            return ControllerResult::failure(QString::fromWCharArray(L"\u8be5\u6309\u952e\u5df2\u88ab\u8bbe\u7f6e\u4e3a\u5f53\u524d\u5b8f\u7684\u6682\u505c\u952e\u3002"));
        }
    }
    return ControllerResult::success();
}

ControllerResult MacroController::validateTriggerKey(const MacroConfig& macro, const KeyDescriptor& key) const {
    if (auto validation = ActiveConfigValidator::validateKeyCombination(key); !validation.ok) return validation;
    if (auto validation = validateKeyAgainstPauseKeys(key, macro); !validation.ok) return validation;
    if (key.device == InputDevice::Mouse && (key.code == VK_LBUTTON || key.code == VK_RBUTTON)) {
        return ControllerResult::failure(QString::fromWCharArray(L"\u542f\u505c\u5feb\u6377\u952e\u4e0d\u80fd\u8bbe\u7f6e\u4e3a\u9f20\u6807\u5de6\u952e\u6216\u53f3\u952e\u3002"));
    }

    const bool stepConflict = std::any_of(macro.steps.begin(), macro.steps.end(),
        [&](const MacroStep& step) { return step.key == key; });
    if (stepConflict) {
        return ControllerResult::failure(QString::fromWCharArray(L"\u8be5\u6309\u952e\u5df2\u88ab\u5f53\u524d\u5b8f\u7684\u6b65\u9aa4\u4e2d\u4f7f\u7528\u3002"));
    }
    const bool pauseConflict = std::any_of(macro.pauseKeys.begin(), macro.pauseKeys.end(),
        [&](const KeyDescriptor& pauseKey) { return pauseKey == key; });
    if (pauseConflict) {
        return ControllerResult::failure(QString::fromWCharArray(L"\u8be5\u6309\u952e\u5df2\u88ab\u5f53\u524d\u5b8f\u7684\u6682\u505c\u952e\u4f7f\u7528\u3002"));
    }
    return ControllerResult::success();
}

ControllerResult MacroController::addStep(const MacroStep& value, std::uint64_t& createdStepId, int insertIndex) {
    auto* macro = activeMacro();
    if (!macro) return ControllerResult::failure(QString::fromWCharArray(L"循环不存在。"));
    MacroStep step = value;
    if (macro->steps.size() >= kMaxMacroSteps)
        return ControllerResult::failure(QStringLiteral("循环最多包含 512 步。"));
    NormalizeMacroStep(step);
    if (MacroActionNeedsKey(step.action)) {
        auto validation = validateStepKey(*macro, step.key);
        if (!validation.ok) return validation;
    }
    step.id = nextStepId(*macro);
    createdStepId = step.id;
    const int count = static_cast<int>(macro->steps.size());
    if (insertIndex < 0 || insertIndex > count) macro->steps.push_back(std::move(step));
    else macro->steps.insert(macro->steps.begin() + insertIndex, std::move(step));
    return ControllerResult::success();
}
ControllerResult MacroController::updateStep(std::uint64_t stepId, const MacroStep& value) {
    auto* macro = activeMacro();
    if (!macro) return ControllerResult::failure(QString::fromWCharArray(L"循环不存在。"));
    auto found = std::find_if(macro->steps.begin(), macro->steps.end(),
        [stepId](const auto& step) { return step.id == stepId; });
    if (found == macro->steps.end()) return ControllerResult::failure(QString::fromWCharArray(L"步骤不存在。"));
    MacroStep step = value;
    NormalizeMacroStep(step);
    if (MacroActionNeedsKey(step.action)) {
        auto validation = validateStepKey(*macro, step.key);
        if (!validation.ok) return validation;
    }
    step.id = stepId;
    step.enabled = found->enabled;
    *found = std::move(step);
    return ControllerResult::success();
}
ControllerResult MacroController::appendRecordedSteps(const std::vector<MacroStep>& steps) {
    auto* macro = activeMacro();
    if (!macro) return ControllerResult::failure(QString::fromWCharArray(L"循环不存在。"));
    if (macro->steps.size() + steps.size() > kMaxMacroSteps)
        return ControllerResult::failure(QStringLiteral("循环最多包含 512 步。"));
    auto candidates = macro->steps;
    auto id = nextStepId(*macro);
    for (auto step : steps) {
        NormalizeMacroStep(step);
        if (MacroActionNeedsKey(step.action)) {
            auto validation = validateStepKey(*macro, step.key);
            if (!validation.ok) return validation;
        }
        step.id = id++;
        candidates.push_back(std::move(step));
    }
    const auto error = ValidateMacroSteps(candidates);
    if (!error.empty()) return ControllerResult::failure(QString::fromStdWString(error));
    macro->steps = std::move(candidates);
    return ControllerResult::success();
}

ControllerResult MacroController::setStepEnabled(std::uint64_t stepId, bool enabled) {
    return updateSteps({stepId}, std::nullopt, enabled);
}

ControllerResult MacroController::updateSteps(const std::vector<std::uint64_t>& stepIds, std::optional<int> delayAfterMs,
    std::optional<bool> enabled, std::optional<int> durationMs) {
    auto* macro = activeMacro();
    if (!macro) return ControllerResult::failure(QString::fromWCharArray(L"\u5b8f\u4e0d\u5b58\u5728\u3002"));

    auto candidates = macro->steps;
    bool found = false;
    for (auto& step : candidates) {
        if (std::find(stepIds.begin(), stepIds.end(), step.id) == stepIds.end()) continue;
        found = true;
        if (delayAfterMs && step.action != MacroAction::Wait) step.delayAfterMs = std::max(*delayAfterMs, 0);
        if (durationMs && (step.action == MacroAction::Hold || step.action == MacroAction::Wait)) step.durationMs = std::max(*durationMs, 1);
        if (enabled.has_value()) {
            if (*enabled && MacroActionNeedsKey(step.action)) {
                const auto validation = validateStepKey(*macro, step.key);
                if (!validation.ok) return validation;
            }
            step.enabled = *enabled;
        }
    }
    if (!found) return ControllerResult::failure(QString::fromWCharArray(L"\u6b65\u9aa4\u4e0d\u5b58\u5728\u3002"));
    {
        MacroConfig candidate = *macro;
        candidate.steps = candidates;
        auto validation = ActiveConfigValidator::validateMacroModifierOverlaps(&candidate);
        if (!validation.ok) return validation;
    }
    macro->steps = std::move(candidates);
    return ControllerResult::success();
}

ControllerResult MacroController::deleteStep(std::uint64_t stepId) {
    auto* macro = activeMacro();
    if (!macro) return ControllerResult::failure(QString::fromWCharArray(L"\u5b8f\u4e0d\u5b58\u5728\u3002"));

    const auto oldSize = macro->steps.size();
    macro->steps.erase(
        std::remove_if(macro->steps.begin(), macro->steps.end(),
            [stepId](const MacroStep& step) { return step.id == stepId; }),
        macro->steps.end());
    return oldSize == macro->steps.size()
        ? ControllerResult::failure(QString::fromWCharArray(L"\u6b65\u9aa4\u4e0d\u5b58\u5728\u3002"))
        : ControllerResult::success();
}

ControllerResult MacroController::deleteSteps(const std::vector<std::uint64_t>& stepIds) {
    auto* macro = activeMacro();
    if (!macro) return ControllerResult::failure(QString::fromWCharArray(L"\u5b8f\u4e0d\u5b58\u5728\u3002"));

    const auto oldSize = macro->steps.size();
    macro->steps.erase(
        std::remove_if(macro->steps.begin(), macro->steps.end(),
            [&stepIds](const MacroStep& step) {
                return std::find(stepIds.begin(), stepIds.end(), step.id) != stepIds.end();
            }),
        macro->steps.end());
    return oldSize == macro->steps.size()
        ? ControllerResult::failure(QString::fromWCharArray(L"\u6b65\u9aa4\u4e0d\u5b58\u5728\u3002"))
        : ControllerResult::success();
}

ControllerResult MacroController::duplicateStep(std::uint64_t stepId,
    std::vector<std::uint64_t>& createdStepIds) {
    return duplicateSteps({stepId}, createdStepIds);
}

ControllerResult MacroController::duplicateSteps(const std::vector<std::uint64_t>& stepIds,
    std::vector<std::uint64_t>& createdStepIds) {
    auto* macro = activeMacro();
    if (!macro) return ControllerResult::failure(QString::fromWCharArray(L"\u5b8f\u4e0d\u5b58\u5728\u3002"));

    std::vector<MacroStep> copies;
    int insertIndex = -1;
    for (std::size_t index = 0; index < macro->steps.size(); ++index) {
        const auto& step = macro->steps[index];
        if (std::find(stepIds.begin(), stepIds.end(), step.id) == stepIds.end()) continue;
        MacroStep copy = step;
        copy.id = nextStepId(*macro) + copies.size();
        copies.push_back(std::move(copy));
        insertIndex = static_cast<int>(index) + 1;
    }
    if (copies.empty()) return ControllerResult::failure(QString::fromWCharArray(L"\u6b65\u9aa4\u4e0d\u5b58\u5728\u3002"));
    if (macro->steps.size() + copies.size() > kMaxMacroSteps)
        return ControllerResult::failure(QStringLiteral("循环最多包含 512 步。"));

    createdStepIds.reserve(copies.size());
    for (const auto& step : copies) createdStepIds.push_back(step.id);
    macro->steps.insert(macro->steps.begin() + insertIndex,
        std::make_move_iterator(copies.begin()), std::make_move_iterator(copies.end()));
    return ControllerResult::success();
}

ControllerResult MacroController::moveStep(std::uint64_t stepId, int targetIndex) {
    auto* macro = activeMacro();
    if (!macro) return ControllerResult::failure(QString::fromWCharArray(L"\u5b8f\u4e0d\u5b58\u5728\u3002"));

    const int count = static_cast<int>(macro->steps.size());
    if (targetIndex < 0 || targetIndex >= count) {
        return ControllerResult::failure(QString::fromWCharArray(L"\u6b65\u9aa4\u4e0d\u5b58\u5728\u3002"));
    }
    const auto stepIt = std::find_if(macro->steps.begin(), macro->steps.end(),
        [stepId](const MacroStep& step) { return step.id == stepId; });
    if (stepIt == macro->steps.end()) {
        return ControllerResult::failure(QString::fromWCharArray(L"\u6b65\u9aa4\u4e0d\u5b58\u5728\u3002"));
    }
    const int fromIndex = static_cast<int>(stepIt - macro->steps.begin());
    if (fromIndex == targetIndex) return ControllerResult::success();

    MacroStep step = *stepIt;
    macro->steps.erase(stepIt);
    macro->steps.insert(macro->steps.begin() + targetIndex, std::move(step));
    return ControllerResult::success();
}

ControllerResult MacroController::upsertPauseKey(std::optional<int> index, const KeyDescriptor& key) {
    auto* macro = activeMacro();
    if (!macro) return ControllerResult::failure(QString::fromWCharArray(L"\u5b8f\u4e0d\u5b58\u5728\u3002"));
    if (index && (*index < 0 || *index >= static_cast<int>(macro->pauseKeys.size())))
        return ControllerResult::failure(QStringLiteral("暂停键不存在。"));

    if (auto validation = ActiveConfigValidator::validateKeyCombination(key); !validation.ok) return validation;
    if (auto validation = validatePauseKeyAgainstSteps(key, *macro); !validation.ok) return validation;
    if (macro->triggerKey.has_value() && *macro->triggerKey == key) {
        return ControllerResult::failure(QString::fromWCharArray(L"\u8be5\u6309\u952e\u5df2\u88ab\u8bbe\u7f6e\u4e3a\u5f53\u524d\u5b8f\u7684\u542f\u505c\u5feb\u6377\u952e\u3002"));
    }
    for (int position = 0; position < static_cast<int>(macro->pauseKeys.size()); ++position) {
        if ((!index || position != *index) && macro->pauseKeys[position] == key) {
            return ControllerResult::failure(QString::fromWCharArray(L"\u8be5\u6309\u952e\u5df2\u5728\u6682\u505c\u952e\u5217\u8868\u4e2d\u3002"));
        }
    }
    for (const auto& step : macro->steps) {
        if (step.key == key) {
            return ControllerResult::failure(QString::fromWCharArray(L"\u8be5\u6309\u952e\u5df2\u88ab\u5f53\u524d\u5b8f\u7684\u6b65\u9aa4\u4e2d\u4f7f\u7528\u3002"));
        }
    }

    if (index) macro->pauseKeys[*index] = key;
    else macro->pauseKeys.push_back(key);
    return ControllerResult::success();
}

ControllerResult MacroController::deletePauseKey(int index) {
    auto* macro = activeMacro();
    if (!macro) return ControllerResult::failure(QString::fromWCharArray(L"\u5b8f\u4e0d\u5b58\u5728\u3002"));
    if (index < 0 || index >= static_cast<int>(macro->pauseKeys.size())) {
        return ControllerResult::failure(QString::fromWCharArray(L"\u6682\u505c\u952e\u4e0d\u5b58\u5728\u3002"));
    }

    macro->pauseKeys.erase(macro->pauseKeys.begin() + index);
    return ControllerResult::success();
}

ControllerResult MacroController::setTriggerKey(const KeyDescriptor& key) {
    auto* macro = activeMacro();
    if (!macro) return ControllerResult::failure(QString::fromWCharArray(L"\u5b8f\u4e0d\u5b58\u5728\u3002"));

    auto validation = validateTriggerKey(*macro, key);
    if (!validation.ok) return validation;

    macro->triggerKey = key;
    return ControllerResult::success();
}

ControllerResult MacroController::clearTriggerKey() {
    auto* macro = activeMacro();
    if (!macro) return ControllerResult::failure(QString::fromWCharArray(L"\u5b8f\u4e0d\u5b58\u5728\u3002"));

    macro->triggerKey.reset();
    return ControllerResult::success();
}

ControllerResult MacroController::setTriggerMode(MacroTriggerMode mode) {
    auto* macro = activeMacro();
    if (!macro) return ControllerResult::failure(QString::fromWCharArray(L"\u5b8f\u4e0d\u5b58\u5728\u3002"));
    macro->triggerMode = mode;
    return ControllerResult::success();
}

ControllerResult MacroController::setLoopCount(int count) {
    auto* macro = activeMacro();
    if (!macro) return ControllerResult::failure(QString::fromWCharArray(L"\u5b8f\u4e0d\u5b58\u5728\u3002"));
    macro->loopCount = count;
    return ControllerResult::success();
}
