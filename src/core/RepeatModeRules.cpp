#include "RepeatModeRules.h"

bool CanRepeatBindingsRunTogether(
    const KeyBinding& first, const KeyBinding& second, bool mixedMode) noexcept {
    if (!first.enabled || !second.enabled) return false;
    if (first.mode == RepeatMode::Independent && second.mode == RepeatMode::Independent) return false;
    if (first.mode == RepeatMode::Sequence && second.mode == RepeatMode::Sequence) return false;

    const auto takesPriority = [mixedMode](const KeyBinding& binding) {
        return mixedMode && binding.mode == RepeatMode::Hold && !binding.suppressMixedMode;
    };
    if (takesPriority(first) && second.mode != RepeatMode::Hold) return false;
    if (takesPriority(second) && first.mode != RepeatMode::Hold) return false;
    return true;
}

RepeatBindingConflict CheckRepeatBindingPair(
    const KeyBinding& first, const KeyBinding& second, bool mixedMode) noexcept {
    if (first.key == second.key && first.mode == second.mode) return RepeatBindingConflict::Duplicate;
    if (!first.enabled || !second.enabled) return RepeatBindingConflict::None;
    const auto inputTriggered = [](RepeatMode mode) {
        return mode == RepeatMode::Hold || mode == RepeatMode::Independent;
    };
    if (first.key == second.key && inputTriggered(first.mode) && inputTriggered(second.mode)) {
        return RepeatBindingConflict::SharedTrigger;
    }
    const auto firstModifier = ModifierMaskFromKey(first.key);
    const auto secondModifier = ModifierMaskFromKey(second.key);
    const auto heldTrigger = [&inputTriggered](const KeyBinding& held, const KeyBinding& trigger) {
        return held.mode == RepeatMode::LongPress && inputTriggered(trigger.mode)
            && ModifierMaskFromKey(held.key) == 0
            && held.key.device == trigger.key.device && held.key.code == trigger.key.code
            && (trigger.key.modifiers & held.key.modifiers) == trigger.key.modifiers;
    };
    if (heldTrigger(first, second) || heldTrigger(second, first)) return RepeatBindingConflict::HeldTrigger;
    if ((first.mode == RepeatMode::Independent && firstModifier != 0 && inputTriggered(second.mode)
            && (second.key.modifiers & firstModifier) != 0)
        || (second.mode == RepeatMode::Independent && secondModifier != 0 && inputTriggered(first.mode)
            && (first.key.modifiers & secondModifier) != 0)) {
        return RepeatBindingConflict::ModifierTrigger;
    }
    if (!CanRepeatBindingsRunTogether(first, second, mixedMode)) return RepeatBindingConflict::None;

    if (first.key.device == second.key.device && first.key.code == second.key.code) {
        return RepeatBindingConflict::SharedPrimary;
    }

    const auto checkModifier = [mixedMode](const KeyBinding& modifier, std::uint8_t mask,
                                 const KeyBinding& other, std::uint8_t otherMask) {
        if (modifier.mode == RepeatMode::LongPress) {
            if (mixedMode && modifier.martialAssist && (other.key.modifiers & mask) != 0) {
                return RepeatBindingConflict::PulsedModifier;
            }
            // A standalone held modifier deliberately supplies a shared modifier layer.
            if (otherMask != 0) {
                return other.mode != RepeatMode::LongPress && (mask & otherMask) != 0
                    ? RepeatBindingConflict::PulsedModifier : RepeatBindingConflict::None;
            }
            return other.key.modifiers == 0 || (other.key.modifiers & mask) == mask
                ? RepeatBindingConflict::None : RepeatBindingConflict::ModifierState;
        }
        return other.key.modifiers != 0
            ? RepeatBindingConflict::PulsedModifier : RepeatBindingConflict::None;
    };
    if (secondModifier != 0 && second.mode == RepeatMode::LongPress) {
        return checkModifier(second, secondModifier, first, firstModifier);
    }
    if (firstModifier != 0) return checkModifier(first, firstModifier, second, secondModifier);
    if (secondModifier != 0) return checkModifier(second, secondModifier, first, firstModifier);

    return first.key.modifiers == second.key.modifiers
        ? RepeatBindingConflict::None : RepeatBindingConflict::ModifierState;
}
