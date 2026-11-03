#include "MacroActions.h"
#include <algorithm>
#include <cstdlib>

bool MacroActionNeedsKey(MacroAction action) {
    return action == MacroAction::Click || action == MacroAction::Hold
        || action == MacroAction::Down || action == MacroAction::Up;
}
bool MacroActionCanPosition(MacroAction action) {
    return action == MacroAction::Click || action == MacroAction::Hold || action == MacroAction::Down;
}
std::string MacroActionStorage(MacroAction action) {
    switch (action) {
    case MacroAction::Click: return "click";
    case MacroAction::Hold: return "hold";
    case MacroAction::Down: return "down";
    case MacroAction::Up: return "up";
    case MacroAction::Wait: return "wait";
    case MacroAction::Wheel: return "wheel";
    }
    return {};
}
std::optional<MacroAction> ParseMacroAction(std::string_view value) {
    if (value == "click") return MacroAction::Click;
    if (value == "hold") return MacroAction::Hold;
    if (value == "down") return MacroAction::Down;
    if (value == "up") return MacroAction::Up;
    if (value == "wait") return MacroAction::Wait;
    if (value == "wheel") return MacroAction::Wheel;
    return std::nullopt;
}
std::wstring MacroActionLabel(MacroAction action) {
    switch (action) {
    case MacroAction::Click: return L"点击";
    case MacroAction::Hold: return L"保持";
    case MacroAction::Down: return L"按下";
    case MacroAction::Up: return L"松开";
    case MacroAction::Wait: return L"等待";
    case MacroAction::Wheel: return L"滚轮";
    }
    return {};
}
std::wstring MacroStepText(const MacroStep& step, const std::wstring& keyName) {
    if (step.action == MacroAction::Wait) return L"等待 " + std::to_wstring(step.durationMs) + L"ms";
    if (step.action == MacroAction::Wheel) return std::wstring(step.wheelDelta > 0 ? L"滚轮向上 " : L"滚轮向下 ")
        + std::to_wstring(std::abs(static_cast<long long>(step.wheelDelta)) / 120) + L" 格";
    auto text = MacroActionLabel(step.action) + L" " + keyName;
    if (step.action == MacroAction::Hold) text += L" " + std::to_wstring(step.durationMs) + L"ms";
    return text;
}
std::uint64_t MacroStepDurationMs(const MacroStep& step) {
    if (step.action == MacroAction::Wait) return static_cast<std::uint64_t>(std::max(1, step.durationMs));
    const auto delay = static_cast<std::uint64_t>(std::max(0, step.delayAfterMs));
    if (step.action == MacroAction::Click) return kInputPulseWidthMs + delay;
    if (step.action == MacroAction::Hold) return static_cast<std::uint64_t>(std::max(1, step.durationMs)) + delay;
    return delay;
}
void NormalizeMacroStep(MacroStep& step) {
    step.delayAfterMs = std::max(0, step.delayAfterMs);
    step.durationMs = std::max(1, step.durationMs);
    if (!MacroActionNeedsKey(step.action)) step.key = {};
    if (!MacroActionCanPosition(step.action) || step.key.device != InputDevice::Mouse) step.clickPos = {};
    if (step.action == MacroAction::Wait) step.delayAfterMs = 0;
}
