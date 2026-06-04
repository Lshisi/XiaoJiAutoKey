#pragma once

#include "AppTypes.h"
#include "controllers/ControllerResult.h"

#include <cstdint>

class MacroController {
public:
    MacroController(AppConfig& config, std::uint64_t& activeMacroId);

    [[nodiscard]] MacroConfig* activeMacro();
    [[nodiscard]] const MacroConfig* activeMacro() const;
    [[nodiscard]] std::uint64_t nextMacroId() const;
    [[nodiscard]] static std::uint64_t nextMacroId(const ProfileConfig& profile);

    void resetActiveMacro();
    void ensureActiveMacro();
    ControllerResult switchMacro(std::uint64_t macroId);
    ControllerResult addMacro(const QString& name, std::uint64_t& createdMacroId);
    ControllerResult renameActiveMacro(const QString& name);
    ControllerResult deleteActiveMacro(std::uint64_t& newActiveMacroId);

    ControllerResult addStep(const KeyDescriptor& key, int delayAfterMs);
    ControllerResult updateStep(int stepIndex, const KeyDescriptor& key, int delayAfterMs);
    ControllerResult deleteStep(int stepIndex);
    ControllerResult moveStep(int fromIndex, int toIndex);

    ControllerResult setTriggerKey(const KeyDescriptor& key);
    ControllerResult setTriggerMode(MacroTriggerMode mode);
    ControllerResult setLoopCount(int count);

private:
    [[nodiscard]] ProfileConfig* activeProfile();
    [[nodiscard]] const ProfileConfig* activeProfile() const;
    [[nodiscard]] ControllerResult validateStepKey(const MacroConfig& macro, const KeyDescriptor& key) const;
    [[nodiscard]] ControllerResult validateTriggerKey(const MacroConfig& macro, const KeyDescriptor& key) const;

    AppConfig& config_;
    std::uint64_t& activeMacroId_;
};
