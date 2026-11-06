#pragma once

#include "AppTypes.h"

enum class RepeatBindingConflict {
    None,
    Duplicate,
    SharedTrigger,
    HeldTrigger,
    ModifierTrigger,
    SharedPrimary,
    PulsedModifier,
    ModifierState,
};

[[nodiscard]] bool CanRepeatBindingsRunTogether(
    const KeyBinding& first, const KeyBinding& second, bool mixedMode) noexcept;
[[nodiscard]] RepeatBindingConflict CheckRepeatBindingPair(
    const KeyBinding& first, const KeyBinding& second, bool mixedMode) noexcept;
