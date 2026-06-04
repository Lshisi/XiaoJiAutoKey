#pragma once

#include "AppTypes.h"

#include <cstdint>

class ConfigNormalizer {
public:
    static void normalize(AppConfig& config, std::uint64_t& activeMacroId);

private:
    static std::uint64_t nextBindingId(const ProfileConfig& profile);
    static std::uint64_t nextMacroId(const ProfileConfig& profile);
};
