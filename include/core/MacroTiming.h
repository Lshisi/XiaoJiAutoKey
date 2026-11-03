#pragma once

#include "AppTypes.h"

#include <cstdint>
#include <vector>

struct MacroTimingEstimate {
    int enabledStepCount = 0;
    int totalStepCount = 0;
    std::uint64_t loopDurationMs = 0;
    bool finiteDuration = false;
    std::uint64_t totalDurationMs = 0;
};

[[nodiscard]] MacroTimingEstimate EstimateMacroTiming(const MacroConfig& macro);
[[nodiscard]] std::wstring FormatMacroDuration(std::uint64_t milliseconds);
[[nodiscard]] std::wstring FormatMacroTimingSummary(const MacroConfig& macro);
