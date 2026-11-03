#include "MacroActions.h"
#include <algorithm>

std::wstring ValidateMacroSteps(const std::vector<MacroStep>& steps) {
    if (steps.size() > kMaxMacroSteps) return L"循环最多包含 512 步。";
    std::vector<KeyDescriptor> held;
    bool hasTime = false;
    bool hasEnabled = false;
    for (std::size_t index = 0; index < steps.size(); ++index) {
        const auto& step = steps[index];
        if (!step.enabled) continue;
        hasEnabled = true;
        const auto fail = [index](const wchar_t* text) { return L"第 " + std::to_wstring(index + 1) + L" 步：" + text; };
        if (MacroActionStorage(step.action).empty()) return fail(L"动作类型无效。");
        if (MacroActionNeedsKey(step.action) && !step.key.IsValid()) return fail(L"请设置按键。");
        if (MacroActionNeedsKey(step.action)) {
            auto effective = step.key;
            for (const auto& heldKey : held) {
                for (const auto& component : ExpandKeyComponents(heldKey))
                    effective.modifiers |= ModifierMaskFromKey(component);
            }
            if (IsReservedKeyCombination(effective)) return fail(L"组合了 Windows 系统保留快捷键，请修改按键。");
        }
        if ((step.action == MacroAction::Wait || step.action == MacroAction::Hold) && step.durationMs < 1)
            return fail(L"时长必须不小于 1 毫秒。");
        if (step.delayAfterMs < 0) return fail(L"动作后间隔不能小于 0。");
        if (step.action == MacroAction::Wheel && (step.wheelDelta == 0 || step.wheelDelta % 120 != 0
            || step.wheelDelta < -12000 || step.wheelDelta > 12000)) return fail(L"滚轮格数须为 1 到 100。");
        auto found = std::find(held.begin(), held.end(), step.key);
        if ((step.action == MacroAction::Click || step.action == MacroAction::Hold)
            && std::any_of(held.begin(), held.end(), [&step](const auto& key) {
                return SamePrimaryKey(key, step.key);
            })) return fail(L"该主键已由按下步骤保持，无法再次点击或保持；请先松开。");
        if (step.action == MacroAction::Down) {
            if (std::any_of(held.begin(), held.end(), [&step](const auto& key) { return SamePrimaryKey(key, step.key); }))
                return fail(L"该键已按下，请先添加对应松开步骤。");
            held.push_back(step.key);
        } else if (step.action == MacroAction::Up) {
            if (found == held.end()) return fail(L"没有对应的按下步骤。");
            held.erase(found);
        }
        hasTime |= MacroStepDurationMs(step) > 0;
    }
    if (!held.empty()) return L"一轮结束仍有未松开的按键，请补充对应的松开步骤。";
    if (hasEnabled && !hasTime) return L"循环总时长为 0，请添加等待或设置动作后间隔。";
    return {};
}
