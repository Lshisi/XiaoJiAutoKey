#pragma once

#include "AppTypes.h"
#include "controllers/ControllerResult.h"

#include <cstdint>
#include <optional>

class ProfileController {
public:
    explicit ProfileController(AppConfig& config);

    [[nodiscard]] ProfileConfig* activeProfile();
    [[nodiscard]] const ProfileConfig* activeProfile() const;
    [[nodiscard]] ProfileConfig* profileById(std::uint64_t profileId);
    [[nodiscard]] const ProfileConfig* profileById(std::uint64_t profileId) const;

    [[nodiscard]] std::uint64_t nextProfileId() const;
    [[nodiscard]] std::uint64_t nextBindingId() const;
    [[nodiscard]] static std::uint64_t nextBindingId(const ProfileConfig& profile);

    ControllerResult switchProfile(std::uint64_t profileId);
    ControllerResult addProfile(const QString& name, std::uint64_t& createdProfileId);
    ControllerResult renameActiveProfile(const QString& name);
    ControllerResult deleteActiveProfile(std::uint64_t& newActiveProfileId);

    ControllerResult setControlHotkey(const KeyDescriptor& key);
    ControllerResult setMixedMode(bool enabled);

    ControllerResult deleteBinding(std::uint64_t id);
    ControllerResult setBindingEnabled(std::uint64_t id, bool enabled);
    ControllerResult upsertBinding(std::optional<std::uint64_t> bindingId,
        const KeyDescriptor& key, RepeatMode mode, int delayMs, bool enabled);

    ControllerResult addPauseKey(const KeyDescriptor& key);
    ControllerResult deletePauseKey(int index);

private:
    AppConfig& config_;
};
