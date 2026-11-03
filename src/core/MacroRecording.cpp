#include "MacroRecording.h"
#include <algorithm>
#include <limits>
void MacroRecording::clear() { steps_.clear(); held_.clear(); lastTime_.reset(); eventCount_ = 0; wheelRemainder_ = 0; }
void MacroRecording::append(const RecordedInput& input, bool recordPositions) {
    KeyDescriptor key = input.key;
    key.modifiers = 0;
    int wheelDelta = input.wheelDelta;
    if (wheelDelta) {
        wheelRemainder_ += wheelDelta;
        wheelDelta = wheelRemainder_ / 120 * 120;
        wheelRemainder_ %= 120;
        if (!wheelDelta) return;
    }
    if (!input.wheelDelta) {
        auto held = std::find(held_.begin(), held_.end(), key);
        if (input.pressed) {
            if (held != held_.end()) return;
            held_.push_back(key);
        } else {
            if (held == held_.end()) return;
            held_.erase(held);
        }
    }
    if (lastTime_ && input.timeMs > *lastTime_) {
        MacroStep wait;
        wait.action = MacroAction::Wait;
        wait.durationMs = static_cast<int>(std::min<std::uint64_t>(input.timeMs - *lastTime_, std::numeric_limits<int>::max()));
        wait.delayAfterMs = 0;
        steps_.push_back(wait);
    }
    MacroStep step;
    step.action = input.wheelDelta ? MacroAction::Wheel : input.pressed ? MacroAction::Down : MacroAction::Up;
    step.key = key;
    step.wheelDelta = wheelDelta;
    step.delayAfterMs = 0;
    if (recordPositions && input.pressed && key.device == InputDevice::Mouse) step.clickPos = input.position;
    steps_.push_back(step);
    lastTime_ = input.timeMs;
    ++eventCount_;
}
std::vector<MacroStep> MacroRecording::finish(std::uint64_t endMs) {
    auto held = held_;
    if (held.empty() && lastTime_ && endMs > *lastTime_) {
        MacroStep wait;
        wait.action = MacroAction::Wait;
        wait.durationMs = static_cast<int>(std::min<std::uint64_t>(endMs - *lastTime_, std::numeric_limits<int>::max()));
        wait.delayAfterMs = 0;
        steps_.push_back(wait);
        lastTime_ = endMs;
    }
    for (auto key = held.rbegin(); key != held.rend(); ++key) append({*key, false, 0, endMs, {}}, false);
    std::vector<MacroStep> result;
    for (std::size_t index = 0; index < steps_.size(); ++index) {
        auto step = steps_[index];
        if (step.action == MacroAction::Down && index + 2 < steps_.size()
            && steps_[index + 1].action == MacroAction::Wait
            && steps_[index + 2].action == MacroAction::Up && steps_[index + 2].key == step.key) {
            step.durationMs = steps_[index + 1].durationMs;
            step.action = MacroAction::Hold;
            index += 2;
        } else if (step.action == MacroAction::Down && index + 1 < steps_.size()
            && steps_[index + 1].action == MacroAction::Up && steps_[index + 1].key == step.key) {
            step.action = MacroAction::Hold;
            step.durationMs = 1;
            ++index;
        }
        result.push_back(step);
    }
    return result;
}
