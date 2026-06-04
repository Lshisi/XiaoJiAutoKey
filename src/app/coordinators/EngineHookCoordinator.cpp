#include "coordinators/EngineHookCoordinator.h"

#include "input/GlobalInputHook.h"
#include "MacroEngine.h"
#include "RepeaterEngine.h"

EngineHookCoordinator::EngineHookCoordinator(QObject* parent) : QObject(parent) {}

void EngineHookCoordinator::bind(Context context) {
    context_ = std::move(context);
    if (!context_.hook) return;

    connect(context_.hook, &GlobalInputHook::capturedKey, this, [this](quint64 encodedKey) {
        if (context_.capturedKey) context_.capturedKey(encodedKey);
    });
    connect(context_.hook, &GlobalInputHook::controlHotkeyPressed, this, [this]() {
        if (context_.toggleRepeatRunning) context_.toggleRepeatRunning();
    });
    connect(context_.hook, &GlobalInputHook::physicalKeyChanged, this, [this](const KeyDescriptor& key, bool pressed) {
        if (!context_.repeaterEngine) return;
        if (pressed) context_.repeaterEngine->OnPhysicalDown(key);
        else context_.repeaterEngine->OnPhysicalUp(key);
    });
    connect(context_.hook, &GlobalInputHook::macroTriggerChanged, this, [this](const KeyDescriptor& key, bool pressed) {
        if (!context_.macroEngine) return;
        if (pressed) context_.macroEngine->OnTriggerDown(key);
        else context_.macroEngine->OnTriggerUp(key);
    });
}

bool EngineHookCoordinator::start() {
    return context_.hook && context_.hook->start();
}

void EngineHookCoordinator::refreshState() {
    if (!context_.hook) return;
    context_.hook->setControlHotkey(context_.controlHotkey ? context_.controlHotkey() : std::nullopt);
    context_.hook->setCaptureActive(context_.captureActive ? context_.captureActive() : false);
    context_.hook->setActiveMode(context_.activeMode ? context_.activeMode() : 0);
    context_.hook->setRepeatRunning(context_.repeatRunning ? context_.repeatRunning() : false);
}
