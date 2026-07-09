#include "config/ActiveConfigValidator.h"

namespace {

bool SameKey(const std::optional<KeyDescriptor>& hotkey, const KeyDescriptor& key) {
    return hotkey.has_value() && *hotkey == key;
}

QString KeyName(const KeyDescriptor& key) {
    return QString::fromStdWString(GetKeyDisplayName(key));
}

ControllerResult failure(const QString& message) {
    return ControllerResult::failure(message);
}

ControllerResult childConflict(const KeyDescriptor& key, const wchar_t* owner) {
    return failure(QString::fromWCharArray(L"\u201c%1\u201d\u5df2\u88ab\u8bbe\u7f6e\u4e3a%2\u7684\u542f\u505c\u5feb\u6377\u952e\uff0c\u4e0d\u80fd\u518d\u4f5c\u4e3a\u5b50\u6309\u952e\u3002")
        .arg(KeyName(key), QString::fromWCharArray(owner)));
}

ControllerResult hotkeyConflict(const KeyDescriptor& key, const wchar_t* hotkeyOwner, const wchar_t* childOwner) {
    return failure(QString::fromWCharArray(L"\u201c%1\u201d\u662f%2\u7684\u542f\u505c\u5feb\u6377\u952e\uff0c\u4f46\u5df2\u88ab\u7528\u4f5c%3\u3002")
        .arg(KeyName(key), QString::fromWCharArray(hotkeyOwner), QString::fromWCharArray(childOwner)));
}

ControllerResult repeatMacroStepConflict(const KeyDescriptor& key) {
    return failure(QString::fromWCharArray(L"\u201c%1\u201d\u5df2\u540c\u65f6\u7528\u4f5c\u6309\u952e\u8fde\u53d1\u6309\u952e\u548c\u5faa\u73af\u6b65\u9aa4\u6309\u952e\uff0c\u8bf7\u6539\u7528\u4e0d\u540c\u6309\u952e\u3002")
        .arg(KeyName(key)));
}

}  // namespace

namespace ActiveConfigValidator {

ControllerResult validateRuntimeKeyConflicts(const ProfileConfig* profile, const MacroConfig* macro) {
    if (!profile || !macro) {
        return ControllerResult::success();
    }

    for (const auto& binding : profile->bindings) {
        if (!binding.enabled) {
            continue;
        }
        for (const auto& step : macro->steps) {
            if (binding.key == step.key) {
                return repeatMacroStepConflict(binding.key);
            }
        }
    }

    return ControllerResult::success();
}

ControllerResult validateRepeatBindingCandidate(
    const KeyDescriptor& key,
    const MacroConfig* macro) {
    if (!macro) {
        return ControllerResult::success();
    }

    for (const auto& step : macro->steps) {
        if (step.key == key) {
            return repeatMacroStepConflict(key);
        }
    }
    return ControllerResult::success();
}

ControllerResult validateMacroStepCandidate(
    const KeyDescriptor& key,
    const ProfileConfig* profile) {
    if (!profile) {
        return ControllerResult::success();
    }

    for (const auto& binding : profile->bindings) {
        if (binding.key == key) {
            return repeatMacroStepConflict(key);
        }
    }
    return ControllerResult::success();
}

ControllerResult validateChildKeyCandidate(
    const KeyDescriptor& key,
    const ProfileConfig* profile,
    const MacroConfig* macro) {
    if (profile && SameKey(profile->controlHotkey, key)) {
        return childConflict(key, L"\u6309\u952e\u8fde\u53d1");
    }
    if (macro && SameKey(macro->triggerKey, key)) {
        return childConflict(key, L"\u5b8f\u5faa\u73af");
    }
    return ControllerResult::success();
}

ControllerResult validateHotkeyCandidate(
    const KeyDescriptor& key,
    const ProfileConfig* profile,
    const MacroConfig* macro) {
    const bool isRepeatHotkey = profile && SameKey(profile->controlHotkey, key);
    const bool isMacroHotkey = macro && SameKey(macro->triggerKey, key);
    const wchar_t* hotkeyOwner = isRepeatHotkey
        ? L"\u6309\u952e\u8fde\u53d1"
        : (isMacroHotkey ? L"\u5b8f\u5faa\u73af" : L"\u542f\u505c");

    if (profile) {
        for (const auto& binding : profile->bindings) {
            if (binding.key == key) {
                return hotkeyConflict(key, hotkeyOwner, L"\u5f53\u524d\u8fde\u53d1\u914d\u7f6e\u7684\u8fde\u53d1/\u6309\u538b/\u72ec\u7acb\u6309\u952e");
            }
        }
        for (const auto& pauseKey : profile->pauseKeys) {
            if (pauseKey == key) {
                return hotkeyConflict(key, hotkeyOwner, L"\u5f53\u524d\u8fde\u53d1\u914d\u7f6e\u7684\u6682\u505c\u952e");
            }
        }
    }

    if (macro) {
        for (const auto& pauseKey : macro->pauseKeys) {
            if (pauseKey == key) {
                return hotkeyConflict(key, hotkeyOwner, L"\u5f53\u524d\u5b8f\u7684\u6682\u505c\u952e");
            }
        }
        for (const auto& step : macro->steps) {
            if (step.key == key) {
                return hotkeyConflict(key, hotkeyOwner, L"\u5f53\u524d\u5b8f\u7684\u6b65\u9aa4\u6309\u952e");
            }
        }
    }
    return ControllerResult::success();
}

ControllerResult validateActiveHotkeys(const ProfileConfig* profile, const MacroConfig* macro) {
    if (profile && profile->controlHotkey.has_value()) {
        auto validation = validateHotkeyCandidate(*profile->controlHotkey, profile, macro);
        if (!validation.ok) return validation;
    }

    if (macro && macro->triggerKey.has_value()) {
        auto validation = validateHotkeyCandidate(*macro->triggerKey, profile, macro);
        if (!validation.ok) return validation;
    }

    return ControllerResult::success();
}

}  // namespace ActiveConfigValidator
