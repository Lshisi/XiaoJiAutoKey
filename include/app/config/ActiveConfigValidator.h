#pragma once

#include "AppTypes.h"
#include "controllers/ControllerResult.h"

namespace ActiveConfigValidator {

[[nodiscard]] ControllerResult validateActiveHotkeys(const ProfileConfig* profile, const MacroConfig* macro);
[[nodiscard]] ControllerResult validateRuntimeKeyConflicts(const ProfileConfig* profile, const MacroConfig* macro);
[[nodiscard]] ControllerResult validateRepeatBindingCandidate(
    const KeyDescriptor& key,
    const MacroConfig* macro);
[[nodiscard]] ControllerResult validateMacroStepCandidate(
    const KeyDescriptor& key,
    const ProfileConfig* profile);
[[nodiscard]] ControllerResult validateHotkeyCandidate(
    const KeyDescriptor& key,
    const ProfileConfig* profile,
    const MacroConfig* macro);
[[nodiscard]] ControllerResult validateChildKeyCandidate(
    const KeyDescriptor& key,
    const ProfileConfig* profile,
    const MacroConfig* macro);

}  // namespace ActiveConfigValidator
