#pragma once
#include "AppTypes.h"
#include <string_view>

[[nodiscard]] bool MacroActionNeedsKey(MacroAction action);
[[nodiscard]] bool MacroActionCanPosition(MacroAction action);
[[nodiscard]] std::string MacroActionStorage(MacroAction action);
[[nodiscard]] std::optional<MacroAction> ParseMacroAction(std::string_view value);
[[nodiscard]] std::wstring MacroActionLabel(MacroAction action);
[[nodiscard]] std::wstring MacroStepText(const MacroStep& step, const std::wstring& keyName);
[[nodiscard]] std::uint64_t MacroStepDurationMs(const MacroStep& step);
[[nodiscard]] std::wstring ValidateMacroSteps(const std::vector<MacroStep>& steps);
void NormalizeMacroStep(MacroStep& step);
