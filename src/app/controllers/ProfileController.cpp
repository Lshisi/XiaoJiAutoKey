#include "controllers/ProfileController.h"

#include "config/ActiveConfigValidator.h"

#include <Windows.h>
#include <algorithm>

namespace {

ControllerResult validateImmediateKeyAgainstBindings(
    const KeyDescriptor& key, const wchar_t* owner, const ProfileConfig& profile) {
    for (const auto& binding : profile.bindings) {
        if (!binding.enabled) continue;
        auto validation = ActiveConfigValidator::validateModifierOverlap(
            key, owner, binding.key, L"连发按键");
        if (!validation.ok) return validation;
    }
    return ControllerResult::success();
}

ControllerResult validateCombinationAgainstImmediateKeys(
    const KeyDescriptor& key, const ProfileConfig& profile) {
    if (profile.controlHotkey.has_value()) {
        auto validation = ActiveConfigValidator::validateModifierOverlap(
            *profile.controlHotkey, L"启停快捷键", key, L"连发按键");
        if (!validation.ok) return validation;
    }
    if (profile.sequenceSwitchKey.has_value()) {
        auto validation = ActiveConfigValidator::validateModifierOverlap(
            *profile.sequenceSwitchKey, L"切换键", key, L"连发按键");
        if (!validation.ok) return validation;
    }
    for (const auto& pauseKey : profile.pauseKeys) {
        auto validation = ActiveConfigValidator::validateModifierOverlap(
            pauseKey, L"暂停键", key, L"连发按键");
        if (!validation.ok) return validation;
    }
    return ControllerResult::success();
}

}

ProfileController::ProfileController(AppConfig& config) : config_(config) {}

ProfileConfig* ProfileController::activeProfile() {
    for (auto& profile : config_.profiles) {
        if (profile.id == config_.activeProfileId) return &profile;
    }
    return config_.profiles.empty() ? nullptr : &config_.profiles.front();
}

const ProfileConfig* ProfileController::activeProfile() const {
    for (const auto& profile : config_.profiles) {
        if (profile.id == config_.activeProfileId) return &profile;
    }
    return config_.profiles.empty() ? nullptr : &config_.profiles.front();
}

ProfileConfig* ProfileController::profileById(std::uint64_t profileId) {
    for (auto& profile : config_.profiles) {
        if (profile.id == profileId) return &profile;
    }
    return nullptr;
}

const ProfileConfig* ProfileController::profileById(std::uint64_t profileId) const {
    for (const auto& profile : config_.profiles) {
        if (profile.id == profileId) return &profile;
    }
    return nullptr;
}

std::uint64_t ProfileController::nextProfileId() const {
    std::uint64_t maxId = 0;
    for (const auto& profile : config_.profiles) maxId = std::max(maxId, profile.id);
    return maxId + 1;
}

std::uint64_t ProfileController::nextBindingId() const {
    if (const auto* profile = activeProfile()) return nextBindingId(*profile);
    return 1;
}

std::uint64_t ProfileController::nextBindingId(const ProfileConfig& profile) {
    std::uint64_t maxId = 0;
    for (const auto& binding : profile.bindings) maxId = std::max(maxId, binding.id);
    return maxId + 1;
}

ControllerResult ProfileController::switchProfile(std::uint64_t profileId) {
    if (!profileById(profileId)) return ControllerResult::failure(QString::fromWCharArray(L"\u914d\u7f6e\u65b9\u6848\u4e0d\u5b58\u5728\u3002"));
    config_.activeProfileId = profileId;
    return ControllerResult::success();
}

ControllerResult ProfileController::addProfile(const QString& name, std::uint64_t& createdProfileId) {
    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty()) return ControllerResult::failure(QString::fromWCharArray(L"\u914d\u7f6e\u540d\u79f0\u4e0d\u80fd\u4e3a\u7a7a\u3002"));

    ProfileConfig profile;
    profile.id = nextProfileId();
    profile.name = trimmed.toStdWString();

    createdProfileId = profile.id;
    config_.profiles.push_back(std::move(profile));
    config_.activeProfileId = createdProfileId;
    return ControllerResult::success();
}

ControllerResult ProfileController::duplicateActiveProfile(const QString& name, std::uint64_t& createdProfileId) {
    const auto* source = activeProfile();
    if (!source) return ControllerResult::failure(QString::fromWCharArray(L"\u6ca1\u6709\u53ef\u7528\u7684\u914d\u7f6e\u65b9\u6848\u3002"));

    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty()) return ControllerResult::failure(QString::fromWCharArray(L"\u914d\u7f6e\u540d\u79f0\u4e0d\u80fd\u4e3a\u7a7a\u3002"));

    ProfileConfig profile = *source;
    profile.id = nextProfileId();
    profile.name = trimmed.toStdWString();
    std::uint64_t nextBindingId = 1;
    for (auto& binding : profile.bindings) {
        binding.id = nextBindingId++;
    }

    createdProfileId = profile.id;
    const auto sourcePosition = std::find_if(config_.profiles.begin(), config_.profiles.end(),
        [source](const ProfileConfig& item) { return item.id == source->id; });
    config_.profiles.insert(sourcePosition + 1, std::move(profile));
    config_.activeProfileId = createdProfileId;
    return ControllerResult::success();
}

ControllerResult ProfileController::renameActiveProfile(const QString& name) {
    auto* profile = activeProfile();
    if (!profile) return ControllerResult::failure(QString::fromWCharArray(L"\u6ca1\u6709\u53ef\u7528\u7684\u914d\u7f6e\u65b9\u6848\u3002"));

    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty()) return ControllerResult::failure(QString::fromWCharArray(L"\u914d\u7f6e\u540d\u79f0\u4e0d\u80fd\u4e3a\u7a7a\u3002"));

    profile->name = trimmed.toStdWString();
    return ControllerResult::success();
}

ControllerResult ProfileController::deleteActiveProfile(std::uint64_t& newActiveProfileId) {
    if (config_.profiles.size() <= 1) {
        return ControllerResult::failure(QString::fromWCharArray(L"\u81f3\u5c11\u9700\u8981\u4fdd\u7559\u4e00\u4e2a\u914d\u7f6e\u65b9\u6848\u3002"));
    }

    auto* profile = activeProfile();
    if (!profile) return ControllerResult::failure(QString::fromWCharArray(L"\u6ca1\u6709\u53ef\u7528\u7684\u914d\u7f6e\u65b9\u6848\u3002"));

    const std::uint64_t deletedId = profile->id;
    config_.profiles.erase(
        std::remove_if(config_.profiles.begin(), config_.profiles.end(),
            [deletedId](const ProfileConfig& item) { return item.id == deletedId; }),
        config_.profiles.end());

    newActiveProfileId = config_.profiles.empty() ? 0 : config_.profiles.front().id;
    config_.activeProfileId = newActiveProfileId;
    return ControllerResult::success();
}

ControllerResult ProfileController::setActiveProfileEnabled(bool enabled) {
    auto* profile = activeProfile();
    if (!profile) return ControllerResult::failure(QString::fromWCharArray(L"\u6ca1\u6709\u53ef\u7528\u7684\u914d\u7f6e\u65b9\u6848\u3002"));

    if (enabled) {
        ProfileConfig candidate = *profile;
        candidate.enabled = true;
        auto validation = ActiveConfigValidator::validateProfileModifierOverlaps(&candidate);
        if (!validation.ok) return validation;
    }

    profile->enabled = enabled;
    return ControllerResult::success();
}

ControllerResult ProfileController::setControlHotkey(const KeyDescriptor& key) {
    auto* profile = activeProfile();
    if (!profile) return ControllerResult::failure(QString::fromWCharArray(L"\u6ca1\u6709\u53ef\u7528\u7684\u914d\u7f6e\u65b9\u6848\u3002"));

    if (auto validation = ActiveConfigValidator::validateKeyCombination(key); !validation.ok) return validation;
    if (auto validation = validateImmediateKeyAgainstBindings(key, L"启停快捷键", *profile); !validation.ok) {
        return validation;
    }
    if (key.device == InputDevice::Mouse && (key.code == VK_LBUTTON || key.code == VK_RBUTTON)) {
        return ControllerResult::failure(QString::fromWCharArray(L"\u542f\u505c\u5feb\u6377\u952e\u4e0d\u80fd\u8bbe\u7f6e\u4e3a\u9f20\u6807\u5de6\u952e\u6216\u53f3\u952e\u3002"));
    }

    const bool conflict = std::any_of(profile->bindings.begin(), profile->bindings.end(),
        [&](const KeyBinding& binding) { return binding.enabled && binding.key == key; });
    if (conflict) {
        return ControllerResult::failure(QString::fromWCharArray(L"\u8be5\u6309\u952e\u5df2\u88ab\u914d\u7f6e\u4e3a\u8fde\u53d1/\u6309\u538b/\u72ec\u7acb/\u5207\u6362\u6309\u952e\u3002"));
    }
    if (profile->sequenceSwitchKey.has_value() && *profile->sequenceSwitchKey == key) {
        return ControllerResult::failure(QString::fromWCharArray(L"\u8be5\u6309\u952e\u5df2\u88ab\u8bbe\u7f6e\u4e3a\u5207\u6362\u952e\u3002"));
    }

    profile->controlHotkey = key;
    return ControllerResult::success();
}

ControllerResult ProfileController::clearControlHotkey() {
    auto* profile = activeProfile();
    if (!profile) return ControllerResult::failure(QString::fromWCharArray(L"\u6ca1\u6709\u53ef\u7528\u7684\u914d\u7f6e\u65b9\u6848\u3002"));

    profile->controlHotkey.reset();
    return ControllerResult::success();
}

ControllerResult ProfileController::setSequenceSwitchKey(const KeyDescriptor& key) {
    auto* profile = activeProfile();
    if (!profile) return ControllerResult::failure(QString::fromWCharArray(L"\u6ca1\u6709\u53ef\u7528\u7684\u914d\u7f6e\u65b9\u6848\u3002"));

    if (auto validation = ActiveConfigValidator::validateKeyCombination(key); !validation.ok) return validation;
    if (auto validation = validateImmediateKeyAgainstBindings(key, L"切换键", *profile); !validation.ok) {
        return validation;
    }
    if (key.device == InputDevice::Mouse && (key.code == VK_LBUTTON || key.code == VK_RBUTTON)) {
        return ControllerResult::failure(QString::fromWCharArray(L"\u5207\u6362\u952e\u4e0d\u80fd\u8bbe\u7f6e\u4e3a\u9f20\u6807\u5de6\u952e\u6216\u53f3\u952e\u3002"));
    }
    if (profile->controlHotkey.has_value() && *profile->controlHotkey == key) {
        return ControllerResult::failure(QString::fromWCharArray(L"\u8be5\u6309\u952e\u5df2\u88ab\u8bbe\u7f6e\u4e3a\u542f\u505c\u5feb\u6377\u952e\u3002"));
    }
    for (const auto& pauseKey : profile->pauseKeys) {
        if (pauseKey == key) {
            return ControllerResult::failure(QString::fromWCharArray(L"\u8be5\u6309\u952e\u5df2\u88ab\u8bbe\u7f6e\u4e3a\u6682\u505c\u952e\u3002"));
        }
    }
    for (const auto& binding : profile->bindings) {
        if (binding.enabled && binding.key == key) {
            return ControllerResult::failure(QString::fromWCharArray(L"\u8be5\u6309\u952e\u5df2\u88ab\u914d\u7f6e\u4e3a\u8fde\u53d1\u6309\u952e\u3002"));
        }
    }

    profile->sequenceSwitchKey = key;
    return ControllerResult::success();
}

ControllerResult ProfileController::clearSequenceSwitchKey() {
    auto* profile = activeProfile();
    if (!profile) return ControllerResult::failure(QString::fromWCharArray(L"\u6ca1\u6709\u53ef\u7528\u7684\u914d\u7f6e\u65b9\u6848\u3002"));
    profile->sequenceSwitchKey.reset();
    return ControllerResult::success();
}

ControllerResult ProfileController::setMixedMode(bool enabled) {
    auto* profile = activeProfile();
    if (!profile) return ControllerResult::failure(QString::fromWCharArray(L"\u6ca1\u6709\u53ef\u7528\u7684\u914d\u7f6e\u65b9\u6848\u3002"));
    profile->mixedMode = enabled;
    return ControllerResult::success();
}

ControllerResult ProfileController::setWasdProtection(bool enabled) {
    auto* profile = activeProfile();
    if (!profile) return ControllerResult::failure(QString::fromWCharArray(L"\u6ca1\u6709\u53ef\u7528\u7684\u914d\u7f6e\u65b9\u6848\u3002"));
    profile->wasdProtection = enabled;
    return ControllerResult::success();
}

ControllerResult ProfileController::setRememberSequencePosition(bool enabled) {
    auto* profile = activeProfile();
    if (!profile) return ControllerResult::failure(QString::fromWCharArray(L"\u6ca1\u6709\u53ef\u7528\u7684\u914d\u7f6e\u65b9\u6848\u3002"));
    profile->rememberSequencePosition = enabled;
    return ControllerResult::success();
}

ControllerResult ProfileController::deleteBinding(std::uint64_t id) {
    return deleteBindings({id});
}

ControllerResult ProfileController::deleteBindings(const std::vector<std::uint64_t>& ids) {
    auto* profile = activeProfile();
    if (!profile) return ControllerResult::failure(QString::fromWCharArray(L"\u6ca1\u6709\u53ef\u7528\u7684\u914d\u7f6e\u65b9\u6848\u3002"));

    const auto oldSize = profile->bindings.size();
    profile->bindings.erase(
        std::remove_if(profile->bindings.begin(), profile->bindings.end(),
            [&ids](const KeyBinding& binding) {
                return std::find(ids.begin(), ids.end(), binding.id) != ids.end();
            }),
        profile->bindings.end());
    return oldSize == profile->bindings.size()
        ? ControllerResult::failure(QString::fromWCharArray(L"\u6309\u952e\u914d\u7f6e\u4e0d\u5b58\u5728\u3002"))
        : ControllerResult::success();
}

ControllerResult ProfileController::setBindingEnabled(std::uint64_t id, bool enabled) {
    auto* profile = activeProfile();
    if (!profile) return ControllerResult::failure(QString::fromWCharArray(L"\u6ca1\u6709\u53ef\u7528\u7684\u914d\u7f6e\u65b9\u6848\u3002"));

    for (auto& binding : profile->bindings) {
        if (binding.id == id) {
            if (enabled) {
                if (auto validation = validateCombinationAgainstImmediateKeys(binding.key, *profile); !validation.ok) {
                    return validation;
                }
                if (profile->controlHotkey.has_value() && *profile->controlHotkey == binding.key) {
                    return ControllerResult::failure(QString::fromWCharArray(L"\u8be5\u6309\u952e\u5df2\u88ab\u8bbe\u7f6e\u4e3a\u542f\u505c\u5feb\u6377\u952e\u3002"));
                }
                if (profile->sequenceSwitchKey.has_value() && *profile->sequenceSwitchKey == binding.key) {
                    return ControllerResult::failure(QString::fromWCharArray(L"\u8be5\u6309\u952e\u5df2\u88ab\u8bbe\u7f6e\u4e3a\u5207\u6362\u952e\u3002"));
                }
                for (const auto& pauseKey : profile->pauseKeys) {
                    if (pauseKey == binding.key) {
                        return ControllerResult::failure(QString::fromWCharArray(L"\u8be5\u6309\u952e\u5df2\u88ab\u8bbe\u7f6e\u4e3a\u6682\u505c\u952e\u3002"));
                    }
                }
                for (const auto& other : profile->bindings) {
                    if (other.id != id && other.enabled && other.key == binding.key) {
                        return ControllerResult::failure(QString::fromWCharArray(L"\u8be5\u6309\u952e\u5df2\u88ab\u914d\u7f6e\u3002"));
                    }
                }
            }
            binding.enabled = enabled;
            return ControllerResult::success();
        }
    }
    return ControllerResult::failure(QString::fromWCharArray(L"\u6309\u952e\u914d\u7f6e\u4e0d\u5b58\u5728\u3002"));
}

ControllerResult ProfileController::upsertBinding(std::optional<std::uint64_t> bindingId,
    const KeyDescriptor& key, const QString& name, RepeatMode mode, int delayMs, bool enabled, bool suppressMixedMode,
    const ClickPosition& clickPos) {
    auto* profile = activeProfile();
    if (!profile) return ControllerResult::failure(QString::fromWCharArray(L"\u6ca1\u6709\u53ef\u7528\u7684\u914d\u7f6e\u65b9\u6848\u3002"));

    if (auto validation = ActiveConfigValidator::validateKeyCombination(key); !validation.ok) return validation;
    if (enabled) {
        if (auto validation = validateCombinationAgainstImmediateKeys(key, *profile); !validation.ok) {
            return validation;
        }
    }
    if (mode != RepeatMode::Hold) suppressMixedMode = false;
    delayMs = std::max(delayMs, kMinDelayMs);
    const std::wstring effectiveName = name.trimmed().toStdWString();
    const ClickPosition effectiveClickPos = key.device == InputDevice::Mouse ? clickPos : ClickPosition{};

    for (const auto& binding : profile->bindings) {
        if (binding.key != key || binding.mode != mode) continue;
        if (bindingId.has_value() && binding.id == *bindingId) continue;
        return ControllerResult::failure(QString::fromWCharArray(L"\u8be5\u6309\u952e\u5728\u5f53\u524d\u6a21\u5f0f\u4e0b\u5df2\u88ab\u914d\u7f6e\u3002"));
    }

    if (enabled) {
        if (profile->controlHotkey.has_value() && *profile->controlHotkey == key) {
            return ControllerResult::failure(QString::fromWCharArray(L"\u8be5\u6309\u952e\u5df2\u88ab\u8bbe\u7f6e\u4e3a\u542f\u505c\u5feb\u6377\u952e\u3002"));
        }

        for (const auto& pauseKey : profile->pauseKeys) {
            if (pauseKey == key) {
                return ControllerResult::failure(QString::fromWCharArray(L"\u8be5\u6309\u952e\u5df2\u88ab\u8bbe\u7f6e\u4e3a\u6682\u505c\u952e\u3002"));
            }
        }

        for (const auto& binding : profile->bindings) {
            if (!binding.enabled || binding.key != key) continue;
            if (bindingId.has_value() && binding.id == *bindingId) continue;
            return ControllerResult::failure(QString::fromWCharArray(L"\u8be5\u6309\u952e\u5df2\u88ab\u914d\u7f6e\u3002"));
        }
        if (profile->sequenceSwitchKey.has_value() && *profile->sequenceSwitchKey == key) {
            return ControllerResult::failure(QString::fromWCharArray(L"\u8be5\u6309\u952e\u5df2\u88ab\u8bbe\u7f6e\u4e3a\u5207\u6362\u952e\u3002"));
        }
    }

    if (bindingId.has_value()) {
        for (auto& binding : profile->bindings) {
            if (binding.id == *bindingId) {
                binding.key = key;
                binding.name = effectiveName;
                binding.mode = mode;
                binding.delayMs = delayMs;
                binding.enabled = enabled;
                binding.suppressMixedMode = suppressMixedMode;
                binding.clickPos = effectiveClickPos;
                return ControllerResult::success();
            }
        }
        return ControllerResult::failure(QString::fromWCharArray(L"\u6309\u952e\u914d\u7f6e\u4e0d\u5b58\u5728\u3002"));
    }

    KeyBinding binding;
    binding.id = nextBindingId(*profile);
    binding.key = key;
    binding.name = effectiveName;
    binding.mode = mode;
    binding.delayMs = delayMs;
    binding.enabled = enabled;
    binding.suppressMixedMode = suppressMixedMode;
    binding.clickPos = effectiveClickPos;
    profile->bindings.push_back(binding);
    return ControllerResult::success();
}

ControllerResult ProfileController::updateBindings(const std::vector<std::uint64_t>& ids, const MacroConfig* activeMacro,
    std::optional<RepeatMode> mode, std::optional<int> delayMs, std::optional<bool> enabled,
    std::optional<bool> participatePriority) {
    auto* profile = activeProfile();
    if (!profile) return ControllerResult::failure(QString::fromWCharArray(L"\u6ca1\u6709\u53ef\u7528\u7684\u914d\u7f6e\u65b9\u6848\u3002"));

    auto candidates = profile->bindings;
    bool found = false;
    for (auto& binding : candidates) {
        if (std::find(ids.begin(), ids.end(), binding.id) == ids.end()) continue;
        found = true;
        if (mode.has_value()) binding.mode = *mode;
        if (delayMs.has_value() && binding.mode != RepeatMode::LongPress) {
            binding.delayMs = std::max(*delayMs, kMinDelayMs);
        }
        if (enabled.has_value()) binding.enabled = *enabled;
        if (binding.mode != RepeatMode::Hold) {
            binding.suppressMixedMode = false;
        } else if (participatePriority.has_value()) {
            binding.suppressMixedMode = !*participatePriority;
        }
    }
    if (!found) return ControllerResult::failure(QString::fromWCharArray(L"\u6309\u952e\u914d\u7f6e\u4e0d\u5b58\u5728\u3002"));

    ProfileConfig candidateProfile = *profile;
    candidateProfile.bindings = candidates;
    for (std::size_t index = 0; index < candidates.size(); ++index) {
        const auto& binding = candidates[index];
        const bool selected = std::find(ids.begin(), ids.end(), binding.id) != ids.end();
        if (!selected) continue;
        const QString keyName = QString::fromStdWString(GetKeyDisplayName(binding.key));

        for (std::size_t otherIndex = 0; otherIndex < candidates.size(); ++otherIndex) {
            if (otherIndex == index || candidates[otherIndex].key != binding.key
                || candidates[otherIndex].mode != binding.mode) continue;
            return ControllerResult::failure(QString::fromWCharArray(L"\u201c%1\u201d\u5728\u540c\u4e00\u6a21\u5f0f\u4e0b\u91cd\u590d\uff0c\u65e0\u6cd5\u4fdd\u5b58\u3002").arg(keyName));
        }
        if (!binding.enabled) continue;

        for (std::size_t otherIndex = 0; otherIndex < candidates.size(); ++otherIndex) {
            if (otherIndex == index || !candidates[otherIndex].enabled
                || candidates[otherIndex].key != binding.key) continue;
            return ControllerResult::failure(QString::fromWCharArray(L"\u201c%1\u201d\u5df2\u88ab\u5176\u4ed6\u542f\u7528\u6309\u952e\u4f7f\u7528\uff0c\u65e0\u6cd5\u540c\u65f6\u542f\u7528\u3002").arg(keyName));
        }
        for (const auto& pauseKey : candidateProfile.pauseKeys) {
            if (pauseKey == binding.key) {
                return ControllerResult::failure(QString::fromWCharArray(L"\u201c%1\u201d\u5df2\u88ab\u8bbe\u7f6e\u4e3a\u6682\u505c\u952e\u3002").arg(keyName));
            }
        }
        auto validation = ActiveConfigValidator::validateRepeatBindingCandidate(
            binding.key, true, &candidateProfile, activeMacro);
        if (!validation.ok) return validation;
    }

    auto activeValidation = ActiveConfigValidator::validateActiveHotkeys(&candidateProfile, activeMacro);
    if (!activeValidation.ok) return activeValidation;
    auto modifierValidation = ActiveConfigValidator::validateProfileModifierOverlaps(&candidateProfile);
    if (!modifierValidation.ok) return modifierValidation;
    profile->bindings = std::move(candidates);
    return ControllerResult::success();
}

ControllerResult ProfileController::addPauseKey(const KeyDescriptor& key) {
    auto* profile = activeProfile();
    if (!profile) return ControllerResult::failure(QString::fromWCharArray(L"\u6ca1\u6709\u53ef\u7528\u7684\u914d\u7f6e\u65b9\u6848\u3002"));

    if (auto validation = ActiveConfigValidator::validateKeyCombination(key); !validation.ok) return validation;
    if (auto validation = validateImmediateKeyAgainstBindings(key, L"暂停键", *profile); !validation.ok) {
        return validation;
    }
    if (profile->controlHotkey.has_value() && *profile->controlHotkey == key) {
        return ControllerResult::failure(QString::fromWCharArray(L"\u8be5\u6309\u952e\u5df2\u88ab\u8bbe\u7f6e\u4e3a\u542f\u505c\u5feb\u6377\u952e\u3002"));
    }
    if (profile->sequenceSwitchKey.has_value() && *profile->sequenceSwitchKey == key) {
        return ControllerResult::failure(QString::fromWCharArray(L"\u8be5\u6309\u952e\u5df2\u88ab\u8bbe\u7f6e\u4e3a\u5207\u6362\u952e\u3002"));
    }
    for (const auto& pauseKey : profile->pauseKeys) {
        if (pauseKey == key) {
            return ControllerResult::failure(QString::fromWCharArray(L"\u8be5\u6309\u952e\u5df2\u5728\u6682\u505c\u952e\u5217\u8868\u4e2d\u3002"));
        }
    }
    for (const auto& binding : profile->bindings) {
        if (binding.enabled && binding.key == key) {
            return ControllerResult::failure(QString::fromWCharArray(L"\u8be5\u6309\u952e\u5df2\u88ab\u914d\u7f6e\u4e3a\u8fde\u53d1/\u6309\u538b/\u72ec\u7acb/\u5207\u6362\u6309\u952e\u3002"));
        }
    }

    profile->pauseKeys.push_back(key);
    return ControllerResult::success();
}

ControllerResult ProfileController::deletePauseKey(int index) {
    auto* profile = activeProfile();
    if (!profile) return ControllerResult::failure(QString::fromWCharArray(L"\u6ca1\u6709\u53ef\u7528\u7684\u914d\u7f6e\u65b9\u6848\u3002"));
    if (index < 0 || index >= static_cast<int>(profile->pauseKeys.size())) {
        return ControllerResult::failure(QString::fromWCharArray(L"\u6682\u505c\u952e\u4e0d\u5b58\u5728\u3002"));
    }

    profile->pauseKeys.erase(profile->pauseKeys.begin() + index);
    return ControllerResult::success();
}
