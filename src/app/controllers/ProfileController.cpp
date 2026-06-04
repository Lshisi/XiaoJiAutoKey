#include "controllers/ProfileController.h"

#include <Windows.h>
#include <algorithm>

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
    auto exists = std::any_of(config_.profiles.begin(), config_.profiles.end(),
        [profileId](const ProfileConfig& profile) { return profile.id == profileId; });
    if (!exists) return ControllerResult::failure(QString::fromWCharArray(L"\u914d\u7f6e\u65b9\u6848\u4e0d\u5b58\u5728\u3002"));
    config_.activeProfileId = profileId;
    return ControllerResult::success();
}

ControllerResult ProfileController::addProfile(const QString& name, std::uint64_t& createdProfileId) {
    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty()) return ControllerResult::failure(QString::fromWCharArray(L"\u914d\u7f6e\u540d\u79f0\u4e0d\u80fd\u4e3a\u7a7a\u3002"));

    ProfileConfig profile;
    profile.id = nextProfileId();
    profile.name = trimmed.toStdWString();

    MacroConfig defaultMacro;
    defaultMacro.id = 1;
    defaultMacro.name = L"\u9ed8\u8ba4\u5b8f";
    defaultMacro.enabled = true;
    profile.macros.push_back(defaultMacro);

    createdProfileId = profile.id;
    config_.profiles.push_back(std::move(profile));
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

ControllerResult ProfileController::setControlHotkey(const KeyDescriptor& key) {
    auto* profile = activeProfile();
    if (!profile) return ControllerResult::failure(QString::fromWCharArray(L"\u6ca1\u6709\u53ef\u7528\u7684\u914d\u7f6e\u65b9\u6848\u3002"));

    if (key.device == InputDevice::Mouse && (key.code == VK_LBUTTON || key.code == VK_RBUTTON)) {
        return ControllerResult::failure(QString::fromWCharArray(L"\u542f\u505c\u5feb\u6377\u952e\u4e0d\u80fd\u8bbe\u7f6e\u4e3a\u9f20\u6807\u5de6\u952e\u6216\u53f3\u952e\u3002"));
    }

    const bool conflict = std::any_of(profile->bindings.begin(), profile->bindings.end(),
        [&](const KeyBinding& binding) { return binding.key == key; });
    if (conflict) {
        return ControllerResult::failure(QString::fromWCharArray(L"\u8be5\u6309\u952e\u5df2\u88ab\u914d\u7f6e\u4e3a\u8fde\u53d1/\u6309\u538b\u6309\u952e\u3002"));
    }

    profile->controlHotkey = key;
    return ControllerResult::success();
}

ControllerResult ProfileController::setMixedMode(bool enabled) {
    auto* profile = activeProfile();
    if (!profile) return ControllerResult::failure(QString::fromWCharArray(L"\u6ca1\u6709\u53ef\u7528\u7684\u914d\u7f6e\u65b9\u6848\u3002"));
    profile->mixedMode = enabled;
    return ControllerResult::success();
}

ControllerResult ProfileController::deleteBinding(std::uint64_t id) {
    auto* profile = activeProfile();
    if (!profile) return ControllerResult::failure(QString::fromWCharArray(L"\u6ca1\u6709\u53ef\u7528\u7684\u914d\u7f6e\u65b9\u6848\u3002"));

    const auto oldSize = profile->bindings.size();
    profile->bindings.erase(
        std::remove_if(profile->bindings.begin(), profile->bindings.end(),
            [id](const KeyBinding& binding) { return binding.id == id; }),
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
            binding.enabled = enabled;
            return ControllerResult::success();
        }
    }
    return ControllerResult::failure(QString::fromWCharArray(L"\u6309\u952e\u914d\u7f6e\u4e0d\u5b58\u5728\u3002"));
}

ControllerResult ProfileController::upsertBinding(std::optional<std::uint64_t> bindingId,
    const KeyDescriptor& key, RepeatMode mode, int delayMs, bool enabled) {
    auto* profile = activeProfile();
    if (!profile) return ControllerResult::failure(QString::fromWCharArray(L"\u6ca1\u6709\u53ef\u7528\u7684\u914d\u7f6e\u65b9\u6848\u3002"));

    if (profile->controlHotkey.has_value() && *profile->controlHotkey == key) {
        return ControllerResult::failure(QString::fromWCharArray(L"\u8be5\u6309\u952e\u5df2\u88ab\u8bbe\u7f6e\u4e3a\u542f\u505c\u5feb\u6377\u952e\u3002"));
    }

    for (const auto& binding : profile->bindings) {
        if (binding.key != key) continue;
        if (bindingId.has_value() && binding.id == *bindingId) continue;
        return ControllerResult::failure(QString::fromWCharArray(L"\u8be5\u6309\u952e\u5df2\u88ab\u914d\u7f6e\u3002"));
    }

    if (bindingId.has_value()) {
        for (auto& binding : profile->bindings) {
            if (binding.id == *bindingId) {
                binding.key = key;
                binding.mode = mode;
                binding.delayMs = delayMs;
                binding.enabled = enabled;
                return ControllerResult::success();
            }
        }
        return ControllerResult::failure(QString::fromWCharArray(L"\u6309\u952e\u914d\u7f6e\u4e0d\u5b58\u5728\u3002"));
    }

    KeyBinding binding;
    binding.id = nextBindingId(*profile);
    binding.key = key;
    binding.mode = mode;
    binding.delayMs = delayMs;
    binding.enabled = enabled;
    profile->bindings.push_back(binding);
    return ControllerResult::success();
}

ControllerResult ProfileController::addPauseKey(const KeyDescriptor& key) {
    auto* profile = activeProfile();
    if (!profile) return ControllerResult::failure(QString::fromWCharArray(L"\u6ca1\u6709\u53ef\u7528\u7684\u914d\u7f6e\u65b9\u6848\u3002"));

    if (profile->controlHotkey.has_value() && *profile->controlHotkey == key) {
        return ControllerResult::failure(QString::fromWCharArray(L"\u8be5\u6309\u952e\u5df2\u88ab\u8bbe\u7f6e\u4e3a\u542f\u505c\u5feb\u6377\u952e\u3002"));
    }
    for (const auto& pauseKey : profile->pauseKeys) {
        if (pauseKey == key) {
            return ControllerResult::failure(QString::fromWCharArray(L"\u8be5\u6309\u952e\u5df2\u5728\u6682\u505c\u952e\u5217\u8868\u4e2d\u3002"));
        }
    }
    for (const auto& binding : profile->bindings) {
        if (binding.key == key) {
            return ControllerResult::failure(QString::fromWCharArray(L"\u8be5\u6309\u952e\u5df2\u88ab\u914d\u7f6e\u4e3a\u8fde\u53d1\u6309\u952e\u3002"));
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
