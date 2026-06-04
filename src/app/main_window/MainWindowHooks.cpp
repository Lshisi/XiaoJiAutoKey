#include "MainWindow.h"

#include "config/ActiveConfigValidator.h"
#include "dialogs/AppMessageDialog.h"

#include <stdexcept>

void MainWindow::setupHooks() {
    engineHookCoordinator_.bind(EngineHookCoordinator::Context{
        &inputHook_,
        &engine_,
        &macroEngine_,
        [this]() {
            const bool shouldStart = !engine_.IsToolRunning();
            if (shouldStart) {
                auto validation = ActiveConfigValidator::validateActiveHotkeys(
                    profileController_.activeProfile(),
                    macroController_.activeMacro());
                if (!validation.ok) {
                    AppMessageDialog::warning(this, QString::fromWCharArray(L"\u6309\u952e\u51b2\u7a81"), validation.message);
                    return;
                }
            }
            engineStateCoordinator_.toggleRepeatRunning();
        },
        [this]() {
            auto* profile = profileController_.activeProfile();
            return profile ? profile->controlHotkey : std::nullopt;
        },
        [this]() {
            auto* macro = macroController_.activeMacro();
            return macro ? macro->triggerKey : std::nullopt;
        },
        [this]() { return captureCoordinator_.captureActive(); },
        [this]() { return engine_.IsToolRunning(); },
        [this](quint64 encodedKey) { captureCoordinator_.handleCapturedKey(encodedKey); },
        [this]() {
            return ActiveConfigValidator::validateActiveHotkeys(
                profileController_.activeProfile(),
                macroController_.activeMacro());
        },
        this
    });

    if (!engineHookCoordinator_.start()) {
        throw std::runtime_error("failed to install global hooks");
    }
}
