#include "controllers/MacroController.h"

#include <Windows.h>
#include <algorithm>

MacroController::MacroController(AppConfig& config, std::uint64_t& activeMacroId)
    : config_(config), activeMacroId_(activeMacroId) {}

ProfileConfig* MacroController::activeProfile() {
    for (auto& profile : config_.profiles) {
        if (profile.id == config_.activeProfileId) return &profile;
    }
    return config_.profiles.empty() ? nullptr : &config_.profiles.front();
}

const ProfileConfig* MacroController::activeProfile() const {
    for (const auto& profile : config_.profiles) {
        if (profile.id == config_.activeProfileId) return &profile;
    }
    return config_.profiles.empty() ? nullptr : &config_.profiles.front();
}

MacroConfig* MacroController::activeMacro() {
    auto* profile = activeProfile();
    if (!profile) return nullptr;
    for (auto& macro : profile->macros) {
        if (macro.id == activeMacroId_) return &macro;
    }
    return nullptr;
}

const MacroConfig* MacroController::activeMacro() const {
    const auto* profile = activeProfile();
    if (!profile) return nullptr;
    for (const auto& macro : profile->macros) {
        if (macro.id == activeMacroId_) return &macro;
    }
    return nullptr;
}

std::uint64_t MacroController::nextMacroId() const {
    if (const auto* profile = activeProfile()) return nextMacroId(*profile);
    return 1;
}

std::uint64_t MacroController::nextMacroId(const ProfileConfig& profile) {
    std::uint64_t maxId = 0;
    for (const auto& macro : profile.macros) maxId = std::max(maxId, macro.id);
    return maxId + 1;
}

void MacroController::resetActiveMacro() {
    auto* profile = activeProfile();
    if (!profile) return;

    if (profile->macros.empty()) {
        MacroConfig macro;
        macro.id = 1;
        macro.name = L"\u9ed8\u8ba4\u5b8f";
        macro.enabled = true;
        profile->macros.push_back(macro);
    }

    if (activeMacroId_ == 0 || std::none_of(profile->macros.begin(), profile->macros.end(),
        [this](const MacroConfig& macro) { return macro.id == activeMacroId_; })) {
        activeMacroId_ = profile->macros.front().id;
    }
}

void MacroController::ensureActiveMacro() {
    resetActiveMacro();
}

ControllerResult MacroController::switchMacro(std::uint64_t macroId) {
    auto* profile = activeProfile();
    if (!profile) return ControllerResult::failure(QString::fromWCharArray(L"\u6ca1\u6709\u53ef\u7528\u7684\u914d\u7f6e\u65b9\u6848\u3002"));

    const bool exists = std::any_of(profile->macros.begin(), profile->macros.end(),
        [macroId](const MacroConfig& macro) { return macro.id == macroId; });
    if (!exists) return ControllerResult::failure(QString::fromWCharArray(L"\u5b8f\u4e0d\u5b58\u5728\u3002"));

    activeMacroId_ = macroId;
    return ControllerResult::success();
}

ControllerResult MacroController::addMacro(const QString& name, std::uint64_t& createdMacroId) {
    auto* profile = activeProfile();
    if (!profile) return ControllerResult::failure(QString::fromWCharArray(L"\u6ca1\u6709\u53ef\u7528\u7684\u914d\u7f6e\u65b9\u6848\u3002"));

    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty()) return ControllerResult::failure(QString::fromWCharArray(L"\u5b8f\u540d\u79f0\u4e0d\u80fd\u4e3a\u7a7a\u3002"));

    MacroConfig macro;
    macro.id = nextMacroId(*profile);
    macro.name = trimmed.toStdWString();
    macro.enabled = true;
    createdMacroId = macro.id;

    profile->macros.push_back(std::move(macro));
    activeMacroId_ = createdMacroId;
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
    auto* profile = activeProfile();
    if (!profile) return ControllerResult::failure(QString::fromWCharArray(L"\u6ca1\u6709\u53ef\u7528\u7684\u914d\u7f6e\u65b9\u6848\u3002"));
    if (profile->macros.size() <= 1) {
        return ControllerResult::failure(QString::fromWCharArray(L"\u81f3\u5c11\u9700\u8981\u4fdd\u7559\u4e00\u4e2a\u5b8f\u914d\u7f6e\u3002"));
    }

    const auto* macro = activeMacro();
    if (!macro) return ControllerResult::failure(QString::fromWCharArray(L"\u5b8f\u4e0d\u5b58\u5728\u3002"));

    const std::uint64_t deletedId = macro->id;
    profile->macros.erase(
        std::remove_if(profile->macros.begin(), profile->macros.end(),
            [deletedId](const MacroConfig& item) { return item.id == deletedId; }),
        profile->macros.end());

    newActiveMacroId = profile->macros.empty() ? 0 : profile->macros.front().id;
    activeMacroId_ = newActiveMacroId;
    return ControllerResult::success();
}

ControllerResult MacroController::validateStepKey(const MacroConfig& macro, const KeyDescriptor& key) const {
    if (macro.triggerKey.has_value() && *macro.triggerKey == key) {
        return ControllerResult::failure(QString::fromWCharArray(L"\u8be5\u6309\u952e\u5df2\u88ab\u8bbe\u7f6e\u4e3a\u5f53\u524d\u5b8f\u7684\u542f\u505c\u5feb\u6377\u952e\u3002"));
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
    return ControllerResult::success();
}

ControllerResult MacroController::addStep(const KeyDescriptor& key, int delayAfterMs) {
    auto* macro = activeMacro();
    if (!macro) return ControllerResult::failure(QString::fromWCharArray(L"\u5b8f\u4e0d\u5b58\u5728\u3002"));

    auto validation = validateStepKey(*macro, key);
    if (!validation.ok) return validation;

    if (delayAfterMs <= 0) delayAfterMs = 50;
    macro->steps.push_back({key, delayAfterMs});
    return ControllerResult::success();
}

ControllerResult MacroController::updateStep(int stepIndex, const KeyDescriptor& key, int delayAfterMs) {
    auto* macro = activeMacro();
    if (!macro) return ControllerResult::failure(QString::fromWCharArray(L"\u5b8f\u4e0d\u5b58\u5728\u3002"));
    if (stepIndex < 0 || stepIndex >= static_cast<int>(macro->steps.size())) {
        return ControllerResult::failure(QString::fromWCharArray(L"\u6b65\u9aa4\u4e0d\u5b58\u5728\u3002"));
    }

    auto validation = validateStepKey(*macro, key);
    if (!validation.ok) return validation;

    if (delayAfterMs <= 0) delayAfterMs = 50;
    macro->steps[stepIndex].key = key;
    macro->steps[stepIndex].delayAfterMs = delayAfterMs;
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

ControllerResult MacroController::setTriggerKey(const KeyDescriptor& key) {
    auto* macro = activeMacro();
    if (!macro) return ControllerResult::failure(QString::fromWCharArray(L"\u5b8f\u4e0d\u5b58\u5728\u3002"));

    auto validation = validateTriggerKey(*macro, key);
    if (!validation.ok) return validation;

    macro->triggerKey = key;
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
