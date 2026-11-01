#include "MainWindow.h"

#include "config/ActiveConfigValidator.h"
#include "dialogs/AppMessageDialog.h"

#include <stdexcept>

void MainWindow::setupHooks() {
    rawInput_.registerWindow(reinterpret_cast<HWND>(winId()));
    inputHook_.setSelfInjectedFilter([this](const KeyDescriptor& key, bool pressed) {
        return inputDriver_.ConsumeSelfInjected(key, pressed);
    });
    inputHook_.setPhysicalInputFilter([this](const KeyDescriptor& key) {
        return inputDriver_.IsHeldDown(key);
    });

    engineHookCoordinator_.bind(EngineHookCoordinator::Context{
        &inputHook_,
        &rawInput_,
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
            return profile && profile->enabled ? profile->controlHotkey : std::nullopt;
        },
        [this]() {
            auto* macro = macroController_.activeMacro();
            return macro && macro->enabled ? macro->triggerKey : std::nullopt;
        },
        [this]() { return captureCoordinator_.captureActive(); },
        [this]() { return engine_.IsToolRunning(); },
        [this](quint64 encodedKey) { captureCoordinator_.handleCapturedKey(encodedKey); },
        [this]() {
            return ActiveConfigValidator::validateActiveHotkeys(
                profileController_.activeProfile(),
                macroController_.activeMacro());
        },
        [this]() {
            macroCoordinator_.pollRunningState();
            refreshStatusOverlay();
        },
        [this]() { return activeInputDriverReady(); },
        [this]() { showDdUnavailableMessage(); },
        [this]() { return config_.driverMode == DriverMode::Usb; },
        this,
        [this]() { return onboardingCoordinator_.blocksStart(); }
    });

    if (!engineHookCoordinator_.start()) {
        throw std::runtime_error("failed to install global hooks");
    }
}
