#include "config/ActiveConfigValidator.h"
#include "MacroActions.h"
#include "RepeatModeRules.h"

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

ControllerResult sequenceSwitchConflict(const KeyDescriptor& key) {
    return failure(QString::fromWCharArray(L"\u201c%1\u201d\u5df2\u88ab\u8bbe\u7f6e\u4e3a\u6309\u952e\u8fde\u53d1\u7684\u5207\u6362\u952e\uff0c\u4e0d\u80fd\u518d\u7528\u4e8e\u5176\u4ed6\u6309\u952e\u529f\u80fd\u3002")
        .arg(KeyName(key)));
}

ControllerResult validateRepeatOutputPair(const KeyBinding& first, const KeyBinding& second, bool mixedMode) {
    const auto conflict = CheckRepeatBindingPair(first, second, mixedMode);
    if (conflict == RepeatBindingConflict::None) return ControllerResult::success();

    QString reason;
    switch (conflict) {
    case RepeatBindingConflict::Duplicate:
        reason = QStringLiteral("同一按键和模式不能重复配置。");
        break;
    case RepeatBindingConflict::SharedTrigger:
        reason = QStringLiteral("同一次物理按键不能同时作为按压触发和独立启停触发。");
        break;
    case RepeatBindingConflict::HeldTrigger:
        reason = QStringLiteral("主键已由长按保持，无法再用同一物理按键可靠地触发按压或独立启停。");
        break;
    case RepeatBindingConflict::ModifierTrigger:
        reason = QStringLiteral("单独修饰键的独立启停会在组合键主键按下前先触发，无法区分这两个操作。");
        break;
    case RepeatBindingConflict::SharedPrimary:
        reason = QStringLiteral("这两种模式会同时输出同一主键，按下、松开会互相干扰。");
        break;
    case RepeatBindingConflict::PulsedModifier:
        reason = QStringLiteral("修饰键的连发会打断另一项需要的按住状态；需要持续按住时请使用长按模式。");
        break;
    case RepeatBindingConflict::ModifierState:
        reason = QStringLiteral("这两种模式会同时输出，但需要不同的修饰键状态，持续按住修饰键会改变另一项的键义。");
        break;
    case RepeatBindingConflict::None:
        break;
    }
    return failure(QStringLiteral("“%1”（%2）与“%3”（%4）冲突：%5")
        .arg(KeyName(first.key), QString::fromStdWString(RepeatModeToText(first.mode)),
            KeyName(second.key), QString::fromStdWString(RepeatModeToText(second.mode)), reason));
}

}  // namespace

namespace ActiveConfigValidator {

ControllerResult validateKeyCombination(const KeyDescriptor& key) {
    if (IsReservedKeyCombination(key)) {
        return failure(QString::fromWCharArray(L"“%1”是 Windows 系统保留组合，不能配置为小叽按键。")
            .arg(KeyName(key)));
    }
    return ControllerResult::success();
}

ControllerResult validateModifierOverlap(
    const KeyDescriptor& immediateKey,
    const wchar_t* immediateOwner,
    const KeyDescriptor& combinationKey,
    const wchar_t* combinationOwner) {
    if (!IsKeyboardModifierKey(immediateKey) || immediateKey.modifiers != 0) {
        return ControllerResult::success();
    }
    const auto modifier = ModifierMaskFromKey(immediateKey);
    if ((combinationKey.modifiers & modifier) == 0) return ControllerResult::success();

    return failure(QString::fromWCharArray(
        L"单独“%1”已用作%2，会在%4“%3”按下主键前先行生效；请更换其中一个。")
        .arg(KeyName(immediateKey), QString::fromWCharArray(immediateOwner),
            KeyName(combinationKey), QString::fromWCharArray(combinationOwner)));
}

ControllerResult validateProfileModifierOverlaps(const ProfileConfig* profile) {
    if (!profile || !profile->enabled) return ControllerResult::success();

    const auto validateImmediate = [&](const KeyDescriptor& immediateKey, const wchar_t* owner) {
        for (const auto& binding : profile->bindings) {
            if (!binding.enabled) continue;
            auto validation = validateModifierOverlap(immediateKey, owner,
                binding.key, L"连发按键");
            if (!validation.ok) return validation;
        }
        return ControllerResult::success();
    };

    if (profile->controlHotkey.has_value()) {
        auto validation = validateImmediate(*profile->controlHotkey, L"启停快捷键");
        if (!validation.ok) return validation;
    }
    if (profile->sequenceSwitchKey.has_value()) {
        auto validation = validateImmediate(*profile->sequenceSwitchKey, L"切换键");
        if (!validation.ok) return validation;
    }
    for (const auto& pauseKey : profile->pauseKeys) {
        auto validation = validateImmediate(pauseKey, L"暂停键");
        if (!validation.ok) return validation;
    }
    for (std::size_t index = 0; index < profile->bindings.size(); ++index) {
        const auto& binding = profile->bindings[index];
        for (std::size_t otherIndex = index + 1; otherIndex < profile->bindings.size(); ++otherIndex) {
            const auto& other = profile->bindings[otherIndex];
            auto validation = validateRepeatOutputPair(binding, other, profile->mixedMode);
            if (!validation.ok) return validation;
        }
    }
    return ControllerResult::success();
}

ControllerResult validateMacroModifierOverlaps(const MacroConfig* macro) {
    if (!macro || !macro->enabled) return ControllerResult::success();
    for (const auto& pauseKey : macro->pauseKeys) {
        if (macro->triggerKey.has_value()) {
            auto validation = validateModifierOverlap(
                pauseKey, L"暂停键", *macro->triggerKey, L"触发快捷键");
            if (!validation.ok) return validation;
        }
        for (const auto& step : macro->steps) {
            if (!step.enabled) continue;
            auto validation = validateModifierOverlap(pauseKey, L"暂停键", step.key, L"循环步骤");
            if (!validation.ok) return validation;
        }
    }
    return ControllerResult::success();
}

ControllerResult validateChildKeyCandidate(
    const KeyDescriptor& key,
    const ProfileConfig* profile,
    const MacroConfig* macro) {
    if (profile && !profile->enabled) profile = nullptr;
    if (macro && !macro->enabled) macro = nullptr;
    if (profile && SameKey(profile->controlHotkey, key)) {
        return childConflict(key, L"\u6309\u952e\u8fde\u53d1");
    }
    if (profile && SameKey(profile->sequenceSwitchKey, key)) {
        return sequenceSwitchConflict(key);
    }
    if (macro && SameKey(macro->triggerKey, key)) {
        return childConflict(key, L"\u5b8f\u5faa\u73af");
    }
    return ControllerResult::success();
}

ControllerResult validateRepeatBindingCandidate(
    const KeyDescriptor& key,
    bool enabled,
    const ProfileConfig* profile,
    const MacroConfig* macro) {
    if (!enabled) {
        return ControllerResult::success();
    }
    return validateChildKeyCandidate(key, profile, macro);
}

ControllerResult validateHotkeyCandidate(
    const KeyDescriptor& key,
    const ProfileConfig* profile,
    const MacroConfig* macro) {
    if (profile && !profile->enabled) profile = nullptr;
    if (macro && !macro->enabled) macro = nullptr;
    const bool isRepeatHotkey = profile && SameKey(profile->controlHotkey, key);
    const bool isMacroHotkey = macro && SameKey(macro->triggerKey, key);
    const wchar_t* hotkeyOwner = isRepeatHotkey
        ? L"\u6309\u952e\u8fde\u53d1"
        : (isMacroHotkey ? L"\u5b8f\u5faa\u73af" : L"\u542f\u505c");

    if (profile && SameKey(profile->sequenceSwitchKey, key)) {
        return sequenceSwitchConflict(key);
    }

    if (profile) {
        for (const auto& binding : profile->bindings) {
            if (binding.enabled && binding.key == key) {
                return hotkeyConflict(key, hotkeyOwner, L"\u5f53\u524d\u8fde\u53d1\u914d\u7f6e\u7684\u8fde\u53d1/\u6309\u538b/\u72ec\u7acb/\u5207\u6362/\u957f\u6309\u6309\u952e");
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

ControllerResult validateSequenceSwitchKeyCandidate(
    const KeyDescriptor& key,
    const ProfileConfig* profile,
    const MacroConfig* macro) {
    if (profile && !profile->enabled) profile = nullptr;
    if (macro && !macro->enabled) macro = nullptr;

    if (profile) {
        if (SameKey(profile->controlHotkey, key)) {
            return failure(QString::fromWCharArray(L"\u201c%1\u201d\u5df2\u88ab\u8bbe\u7f6e\u4e3a\u6309\u952e\u8fde\u53d1\u7684\u542f\u505c\u5feb\u6377\u952e\u3002")
                .arg(KeyName(key)));
        }
        for (const auto& binding : profile->bindings) {
            if (binding.enabled && binding.key == key) {
                return failure(QString::fromWCharArray(L"\u201c%1\u201d\u5df2\u5728\u5f53\u524d\u8fde\u53d1\u914d\u7f6e\u7684\u6309\u952e\u5217\u8868\u4e2d\u4f7f\u7528\u3002")
                    .arg(KeyName(key)));
            }
        }
        for (const auto& pauseKey : profile->pauseKeys) {
            if (pauseKey == key) {
                return failure(QString::fromWCharArray(L"\u201c%1\u201d\u5df2\u88ab\u8bbe\u7f6e\u4e3a\u5f53\u524d\u8fde\u53d1\u914d\u7f6e\u7684\u6682\u505c\u952e\u3002")
                    .arg(KeyName(key)));
            }
        }
    }

    if (macro) {
        if (SameKey(macro->triggerKey, key)) {
            return failure(QString::fromWCharArray(L"\u201c%1\u201d\u5df2\u88ab\u8bbe\u7f6e\u4e3a\u5f53\u524d\u5faa\u73af\u8fde\u53d1\u7684\u542f\u505c\u5feb\u6377\u952e\u3002")
                .arg(KeyName(key)));
        }
        for (const auto& pauseKey : macro->pauseKeys) {
            if (pauseKey == key) {
                return failure(QString::fromWCharArray(L"\u201c%1\u201d\u5df2\u88ab\u8bbe\u7f6e\u4e3a\u5f53\u524d\u5faa\u73af\u8fde\u53d1\u7684\u6682\u505c\u952e\u3002")
                    .arg(KeyName(key)));
            }
        }
        for (const auto& step : macro->steps) {
            if (step.key == key) {
                return failure(QString::fromWCharArray(L"\u201c%1\u201d\u5df2\u5728\u5f53\u524d\u5faa\u73af\u8fde\u53d1\u7684\u6b65\u9aa4\u4e2d\u4f7f\u7528\u3002")
                    .arg(KeyName(key)));
            }
        }
    }

    return ControllerResult::success();
}

ControllerResult validateActiveHotkeys(const ProfileConfig* profile, const MacroConfig* macro) {
    if (profile && !profile->enabled) profile = nullptr;
    if (macro && !macro->enabled) macro = nullptr;
    if (profile && profile->controlHotkey.has_value()) {
        auto validation = validateHotkeyCandidate(*profile->controlHotkey, profile, macro);
        if (!validation.ok) return validation;
    }

    if (macro && macro->triggerKey.has_value()) {
        auto validation = validateHotkeyCandidate(*macro->triggerKey, profile, macro);
        if (!validation.ok) return validation;
    }

    if (profile && profile->sequenceSwitchKey.has_value()) {
        auto validation = validateSequenceSwitchKeyCandidate(*profile->sequenceSwitchKey, profile, macro);
        if (!validation.ok) return validation;
    }

    auto profileModifierValidation = validateProfileModifierOverlaps(profile);
    if (!profileModifierValidation.ok) return profileModifierValidation;
    auto macroModifierValidation = validateMacroModifierOverlaps(macro);
    if (!macroModifierValidation.ok) return macroModifierValidation;

    return ControllerResult::success();
}

}  // namespace ActiveConfigValidator
