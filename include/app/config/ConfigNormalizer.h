#pragma once

#include "AppTypes.h"

#include <cstdint>

class ConfigNormalizer {
public:
    static void normalize(AppConfig& config);

private:
    static std::uint64_t nextBindingId(const ProfileConfig& profile);
    static std::uint64_t nextStepId(const MacroConfig& macro);
    static std::uint64_t nextMacroId(const AppConfig& config);
};
