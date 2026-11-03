#pragma once

#include "AppTypes.h"
#include "controllers/ControllerResult.h"

namespace ActiveConfigValidator {

[[nodiscard]] ControllerResult validateKeyCombination(const KeyDescriptor& key);
[[nodiscard]] ControllerResult validateModifierOverlap(
    const KeyDescriptor& immediateKey,
    const wchar_t* immediateOwner,
    const KeyDescriptor& combinationKey,
    const wchar_t* combinationOwner);
[[nodiscard]] ControllerResult validateProfileModifierOverlaps(const ProfileConfig* profile);
[[nodiscard]] ControllerResult validateMacroModifierOverlaps(const MacroConfig* macro);
[[nodiscard]] ControllerResult validateActiveHotkeys(const ProfileConfig* profile, const MacroConfig* macro);
[[nodiscard]] ControllerResult validateHotkeyCandidate(
    const KeyDescriptor& key,
    const ProfileConfig* profile,
    const MacroConfig* macro);
[[nodiscard]] ControllerResult validateChildKeyCandidate(
    const KeyDescriptor& key,
    const ProfileConfig* profile,
    const MacroConfig* macro);
[[nodiscard]] ControllerResult validateRepeatBindingCandidate(
    const KeyDescriptor& key,
    bool enabled,
    const ProfileConfig* profile,
    const MacroConfig* macro);
[[nodiscard]] ControllerResult validateSequenceSwitchKeyCandidate(
    const KeyDescriptor& key,
    const ProfileConfig* profile,
    const MacroConfig* macro);

}  // namespace ActiveConfigValidator
