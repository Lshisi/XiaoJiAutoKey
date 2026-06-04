#include "MainWindow.h"

#include <stdexcept>

void MainWindow::setupHooks() {
    engineHookCoordinator_.bind(EngineHookCoordinator::Context{
        &inputHook_,
        &engine_,
        &macroEngine_,
        [this]() { engineStateCoordinator_.toggleRepeatRunning(); },
        [this]() {
            auto* profile = profileController_.activeProfile();
            return profile ? profile->controlHotkey : std::nullopt;
        },
        [this]() { return captureCoordinator_.captureActive(); },
        [this]() { return activeMode_.load(); },
        [this]() { return engine_.IsToolRunning(); },
        [this](quint64 encodedKey) { captureCoordinator_.handleCapturedKey(encodedKey); }
    });

    if (!engineHookCoordinator_.start()) {
        throw std::runtime_error("failed to install global hooks");
    }
}
