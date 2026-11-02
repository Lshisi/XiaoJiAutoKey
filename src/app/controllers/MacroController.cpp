#include "controllers/MacroController.h"

#include <Windows.h>
#include <algorithm>

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

ControllerResult MacroController::setActiveMacroNote(const QString& note) {
    auto* macro = activeMacro();
    if (!macro) return ControllerResult::failure(QStringLiteral("循环配置不存在。"));
    macro->note = note.trimmed().toStdWString();
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

    macro->enabled = enabled;
    return ControllerResult::success();
}

ControllerResult MacroController::validateStepKey(const MacroConfig& macro, const KeyDescriptor& key) const {
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

ControllerResult MacroController::addStep(const KeyDescriptor& key, int delayAfterMs,
    const ClickPosition& clickPos) {
    auto* macro = activeMacro();
    if (!macro) return ControllerResult::failure(QString::fromWCharArray(L"\u5b8f\u4e0d\u5b58\u5728\u3002"));

    auto validation = validateStepKey(*macro, key);
    if (!validation.ok) return validation;

    delayAfterMs = std::max(delayAfterMs, kMinDelayMs);
    MacroStep step;
    step.key = key;
    step.delayAfterMs = delayAfterMs;
    step.clickPos = key.device == InputDevice::Mouse ? clickPos : ClickPosition{};
    macro->steps.push_back(std::move(step));
    return ControllerResult::success();
}

ControllerResult MacroController::updateStep(int stepIndex, const KeyDescriptor& key, int delayAfterMs,
    const ClickPosition& clickPos) {
    auto* macro = activeMacro();
    if (!macro) return ControllerResult::failure(QString::fromWCharArray(L"\u5b8f\u4e0d\u5b58\u5728\u3002"));
    if (stepIndex < 0 || stepIndex >= static_cast<int>(macro->steps.size())) {
        return ControllerResult::failure(QString::fromWCharArray(L"\u6b65\u9aa4\u4e0d\u5b58\u5728\u3002"));
    }

    auto validation = validateStepKey(*macro, key);
    if (!validation.ok) return validation;

    delayAfterMs = std::max(delayAfterMs, kMinDelayMs);
    macro->steps[stepIndex].key = key;
    macro->steps[stepIndex].delayAfterMs = delayAfterMs;
    macro->steps[stepIndex].clickPos = key.device == InputDevice::Mouse ? clickPos : ClickPosition{};
    return ControllerResult::success();
}

ControllerResult MacroController::deleteStep(int stepIndex) {
    auto* macro = activeMacro();
    if (!macro) return ControllerResult::failure(QString::fromWCharArray(L"\u5b8f\u4e0d\u5b58\u5728\u3002"));
    if (stepIndex < 0 || stepIndex >= static_cast<int>(macro->steps.size())) {
        return ControllerResult::failure(QString::fromWCharArray(L"\u6b65\u9aa4\u4e0d\u5b58\u5728\u3002"));
    }

    macro->steps.erase(macro->steps.begin() + stepIndex);
    return ControllerResult::success();
}

ControllerResult MacroController::moveStep(int fromIndex, int toIndex) {
    auto* macro = activeMacro();
    if (!macro) return ControllerResult::failure(QString::fromWCharArray(L"\u5b8f\u4e0d\u5b58\u5728\u3002"));

    const int count = static_cast<int>(macro->steps.size());
    if (fromIndex < 0 || fromIndex >= count || toIndex < 0 || toIndex >= count) {
        return ControllerResult::failure(QString::fromWCharArray(L"\u6b65\u9aa4\u4e0d\u5b58\u5728\u3002"));
    }
    if (fromIndex == toIndex) return ControllerResult::success();

    MacroStep step = macro->steps[fromIndex];
    macro->steps.erase(macro->steps.begin() + fromIndex);
    macro->steps.insert(macro->steps.begin() + toIndex, step);
    return ControllerResult::success();
}

ControllerResult MacroController::addPauseKey(const KeyDescriptor& key) {
    auto* macro = activeMacro();
    if (!macro) return ControllerResult::failure(QString::fromWCharArray(L"\u5b8f\u4e0d\u5b58\u5728\u3002"));

    if (macro->triggerKey.has_value() && *macro->triggerKey == key) {
        return ControllerResult::failure(QString::fromWCharArray(L"\u8be5\u6309\u952e\u5df2\u88ab\u8bbe\u7f6e\u4e3a\u5f53\u524d\u5b8f\u7684\u542f\u505c\u5feb\u6377\u952e\u3002"));
    }
    for (const auto& pauseKey : macro->pauseKeys) {
        if (pauseKey == key) {
            return ControllerResult::failure(QString::fromWCharArray(L"\u8be5\u6309\u952e\u5df2\u5728\u6682\u505c\u952e\u5217\u8868\u4e2d\u3002"));
        }
    }
    for (const auto& step : macro->steps) {
        if (step.key == key) {
            return ControllerResult::failure(QString::fromWCharArray(L"\u8be5\u6309\u952e\u5df2\u88ab\u5f53\u524d\u5b8f\u7684\u6b65\u9aa4\u4e2d\u4f7f\u7528\u3002"));
        }
    }

    macro->pauseKeys.push_back(key);
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
