#include "coordinators/EngineStateCoordinator.h"

#include "input/GlobalInputHook.h"
#include "MacroEngine.h"
#include "RepeaterEngine.h"
#include "ui/pages/KeyPage.h"
#include "view/MainWindowView.h"

EngineStateCoordinator::EngineStateCoordinator(QObject* parent) : QObject(parent) {}

void EngineStateCoordinator::bind(Context context) {
    context_ = std::move(context);
}

void EngineStateCoordinator::startThreads() {
    if (context_.repeaterEngine) context_.repeaterEngine->StartThread();
    if (context_.macroEngine) context_.macroEngine->StartThread();
}

void EngineStateCoordinator::stopThreads() {
    if (context_.repeaterEngine) context_.repeaterEngine->StopThread();
    if (context_.macroEngine) context_.macroEngine->StopThread();
}

void EngineStateCoordinator::toggleRepeatRunning() {
    if (!context_.repeaterEngine) return;
    setRepeatRunning(!context_.repeaterEngine->IsToolRunning());
}

void EngineStateCoordinator::setRepeatRunning(bool running) {
    if (!setRepeatRunningFast(running)) return;
    handleRepeatStateChanged(running);
}

bool EngineStateCoordinator::setRepeatRunningFast(bool running) {
    if (!context_.repeaterEngine) return false;
    if (context_.repeaterEngine->IsToolRunning() == running) return false;

    context_.repeaterEngine->SetToolRunning(running);
    if (context_.inputHook) context_.inputHook->setRepeatRunning(running);
    return true;
}

void EngineStateCoordinator::applyBindings() {
    if (!context_.repeaterEngine) return;

    auto* profile = context_.activeProfile ? context_.activeProfile() : nullptr;
    if (!profile) return;

    context_.repeaterEngine->UpdateBindings(profile->bindings);
    context_.repeaterEngine->SetMixedMode(profile->mixedMode);
    context_.repeaterEngine->SetPauseKeys(profile->pauseKeys);

    if (context_.refreshHookState) context_.refreshHookState();
    updateRepeatStatus();
}

void EngineStateCoordinator::applyMacros() {
    if (!context_.macroEngine) return;

    auto* macro = context_.activeMacro ? context_.activeMacro() : nullptr;
    if (macro) {
        context_.macroEngine->UpdateMacros({*macro});
    } else {
        context_.macroEngine->UpdateMacros({});
    }
}

void EngineStateCoordinator::updateRepeatStatus() {
    if (!context_.view || !context_.repeaterEngine) return;
    context_.view->keyPage()->setRunning(context_.repeaterEngine->IsToolRunning());
}

void EngineStateCoordinator::handleRepeatStateChanged(bool running) {
    if (context_.view) context_.view->keyPage()->setRunning(running);
    if (context_.refreshStatusIndicators) context_.refreshStatusIndicators();
    if (context_.playToggleSound) context_.playToggleSound(running, false);
}
