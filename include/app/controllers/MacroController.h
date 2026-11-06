#pragma once

#include "AppTypes.h"
#include "controllers/ControllerResult.h"

#include <cstdint>
#include <optional>
#include <vector>

class MacroController {
public:
    explicit MacroController(AppConfig& config);

    [[nodiscard]] MacroConfig* activeMacro();
    [[nodiscard]] const MacroConfig* activeMacro() const;
    [[nodiscard]] MacroConfig* macroById(std::uint64_t macroId);
    [[nodiscard]] const MacroConfig* macroById(std::uint64_t macroId) const;
    [[nodiscard]] std::uint64_t nextMacroId() const;
    [[nodiscard]] static std::uint64_t nextMacroId(const AppConfig& config);
    [[nodiscard]] static std::uint64_t nextStepId(const MacroConfig& macro);

    void resetActiveMacro();
    void ensureActiveMacro();
    ControllerResult switchMacro(std::uint64_t macroId);
    ControllerResult addMacro(const QString& name, std::uint64_t& createdMacroId);
    ControllerResult duplicateActiveMacro(const QString& name, std::uint64_t& createdMacroId);
    ControllerResult renameActiveMacro(const QString& name);
    ControllerResult deleteActiveMacro(std::uint64_t& newActiveMacroId);

    ControllerResult setActiveMacroEnabled(bool enabled);

    ControllerResult addStep(const MacroStep& step, std::uint64_t& createdStepId, int insertIndex = -1);
    ControllerResult updateStep(std::uint64_t stepId, const MacroStep& step);
    ControllerResult appendRecordedSteps(const std::vector<MacroStep>& steps);
    ControllerResult updateSteps(const std::vector<std::uint64_t>& stepIds, std::optional<int> delayAfterMs,
        std::optional<bool> enabled, std::optional<int> durationMs = std::nullopt);
    ControllerResult setStepEnabled(std::uint64_t stepId, bool enabled);
    ControllerResult deleteStep(std::uint64_t stepId);
    ControllerResult deleteSteps(const std::vector<std::uint64_t>& stepIds);
    ControllerResult duplicateStep(std::uint64_t stepId, std::vector<std::uint64_t>& createdStepIds);
    ControllerResult duplicateSteps(const std::vector<std::uint64_t>& stepIds,
        std::vector<std::uint64_t>& createdStepIds);
    ControllerResult moveStep(std::uint64_t stepId, int targetIndex);
    ControllerResult upsertPauseKey(std::optional<int> index, const KeyDescriptor& key);
    ControllerResult deletePauseKey(int index);

    ControllerResult setTriggerKey(const KeyDescriptor& key);
    ControllerResult clearTriggerKey();
    ControllerResult setTriggerMode(MacroTriggerMode mode);
    ControllerResult setLoopCount(int count);

private:
    [[nodiscard]] ControllerResult validateStepKey(const MacroConfig& macro, const KeyDescriptor& key) const;
    [[nodiscard]] ControllerResult validateTriggerKey(const MacroConfig& macro, const KeyDescriptor& key) const;

    AppConfig& config_;
};
