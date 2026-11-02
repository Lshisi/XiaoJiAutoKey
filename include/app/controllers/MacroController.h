#pragma once

#include "AppTypes.h"
#include "controllers/ControllerResult.h"

#include <cstdint>

class MacroController {
public:
    explicit MacroController(AppConfig& config);

    [[nodiscard]] MacroConfig* activeMacro();
    [[nodiscard]] const MacroConfig* activeMacro() const;
    [[nodiscard]] MacroConfig* macroById(std::uint64_t macroId);
    [[nodiscard]] const MacroConfig* macroById(std::uint64_t macroId) const;
    [[nodiscard]] std::uint64_t nextMacroId() const;
    [[nodiscard]] static std::uint64_t nextMacroId(const AppConfig& config);

    void resetActiveMacro();
    void ensureActiveMacro();
    ControllerResult switchMacro(std::uint64_t macroId);
    ControllerResult addMacro(const QString& name, std::uint64_t& createdMacroId);
    ControllerResult duplicateActiveMacro(const QString& name, std::uint64_t& createdMacroId);
    ControllerResult renameActiveMacro(const QString& name);
    ControllerResult setActiveMacroNote(const QString& note);
    ControllerResult deleteActiveMacro(std::uint64_t& newActiveMacroId);

    ControllerResult setActiveMacroEnabled(bool enabled);

    ControllerResult addStep(const KeyDescriptor& key, const QString& name, int delayAfterMs,
        const ClickPosition& clickPos = {});
    ControllerResult updateStep(int stepIndex, const KeyDescriptor& key, const QString& name, int delayAfterMs,
        const ClickPosition& clickPos = {});
    ControllerResult deleteStep(int stepIndex);
    ControllerResult moveStep(int fromIndex, int toIndex);
    ControllerResult addPauseKey(const KeyDescriptor& key);
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
