#pragma once
#include "AppTypes.h"
#include <cstdint>
#include <vector>

struct RecordedInput {
    KeyDescriptor key;
    bool pressed = false;
    int wheelDelta = 0;
    std::uint64_t timeMs = 0;
    ClickPosition position;
};
class MacroRecording {
public:
    explicit MacroRecording(std::size_t stepLimit = kMaxMacroSteps) : stepLimit_(stepLimit) {}
    void clear();
    void append(const RecordedInput& input, bool recordPositions);
    [[nodiscard]] std::vector<MacroStep> finish(std::uint64_t endMs);
    [[nodiscard]] std::size_t eventCount() const { return eventCount_; }
    [[nodiscard]] bool atCapacity() const { return steps_.size() + held_.size() + 3 >= stepLimit_; }
private:
    std::size_t stepLimit_;
    std::vector<MacroStep> steps_;
    std::vector<KeyDescriptor> held_;
    std::optional<std::uint64_t> lastTime_;
    std::size_t eventCount_ = 0;
    int wheelRemainder_ = 0;
};
