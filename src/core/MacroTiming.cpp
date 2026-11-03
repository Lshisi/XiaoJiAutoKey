#include "MacroTiming.h"
#include "MacroActions.h"

#include <algorithm>
#include <array>
#include <limits>

namespace {
bool multiplyChecked(std::uint64_t value, int count, std::uint64_t& result) {
    if (value == 0 || count <= 0) {
        result = 0;
        return true;
    }
    if (value > std::numeric_limits<std::uint64_t>::max() / static_cast<std::uint64_t>(count))
        return false;
    result = value * static_cast<std::uint64_t>(count);
    return true;
}
}

MacroTimingEstimate EstimateMacroTiming(const MacroConfig& macro) {
    MacroTimingEstimate estimate;
    estimate.totalStepCount = static_cast<int>(macro.steps.size());
    for (const auto& step : macro.steps) {
        if (!step.enabled) continue;
        ++estimate.enabledStepCount;
        const auto stepDuration = MacroStepDurationMs(step);
        if (stepDuration > std::numeric_limits<std::uint64_t>::max() - estimate.loopDurationMs)
            return estimate;
        estimate.loopDurationMs += stepDuration;
    }

    const bool finiteToggle = macro.triggerMode == MacroTriggerMode::Toggle
        && macro.loopCount > 0 && estimate.enabledStepCount > 0;
    estimate.finiteDuration = finiteToggle
        && multiplyChecked(estimate.loopDurationMs, macro.loopCount, estimate.totalDurationMs);
    return estimate;
}

std::wstring FormatMacroDuration(std::uint64_t milliseconds) {
    constexpr std::uint64_t kMinuteMs = 60'000;
    if (milliseconds < 1'000) {
        wchar_t buffer[32]{};
        swprintf(buffer, std::size(buffer), L"%llu 毫秒",
            static_cast<unsigned long long>(milliseconds));
        return buffer;
    }
    if (milliseconds < kMinuteMs) {
        const auto seconds = static_cast<double>(milliseconds) / 1000.0;
        wchar_t buffer[32]{};
        swprintf(buffer, std::size(buffer), L"%.1f 秒", seconds);
        return buffer;
    }

    const auto minutes = milliseconds / kMinuteMs;
    const auto seconds = (milliseconds % kMinuteMs + 500) / 1000;
    if (minutes < 60) {
        wchar_t buffer[48]{};
        swprintf(buffer, std::size(buffer), L"%llu 分 %02llu 秒",
            static_cast<unsigned long long>(minutes), static_cast<unsigned long long>(seconds));
        return buffer;
    }

    const auto hours = minutes / 60;
    const auto remainingMinutes = minutes % 60;
    wchar_t buffer[64]{};
    swprintf(buffer, std::size(buffer), L"%llu 小时 %02llu 分",
        static_cast<unsigned long long>(hours), static_cast<unsigned long long>(remainingMinutes));
    return buffer;
}

std::wstring FormatMacroTimingSummary(const MacroConfig& macro) {
    const auto estimate = EstimateMacroTiming(macro);
    if (estimate.totalStepCount == 0)
        return L"0 步 · 未配置步骤";
    if (estimate.enabledStepCount == 0)
        return L"0/" + std::to_wstring(estimate.totalStepCount) + L" 步 · 未启用步骤";

    std::wstring steps = std::to_wstring(estimate.enabledStepCount);
    if (estimate.enabledStepCount != estimate.totalStepCount)
        steps += L"/" + std::to_wstring(estimate.totalStepCount);

    std::wstring result = steps + L" 步 · 单轮预计 "
        + FormatMacroDuration(estimate.loopDurationMs);
    if (macro.triggerMode == MacroTriggerMode::Hold)
        return result + L" · 按住期间持续执行";
    if (macro.loopCount <= 0)
        return result + L" · 持续循环";
    if (!estimate.finiteDuration)
        return result + L" · 总时长过长";
    return result + L" · " + std::to_wstring(macro.loopCount) + L" 轮预计 "
        + FormatMacroDuration(estimate.totalDurationMs);
}
