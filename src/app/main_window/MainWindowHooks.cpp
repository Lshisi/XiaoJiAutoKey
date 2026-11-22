#include "MainWindow.h"

#include "config/ActiveConfigValidator.h"
#include "dialogs/AppMessageDialog.h"

#include <stdexcept>

void MainWindow::setupHooks() {
    engine_.SetPauseChangedCallback([this](bool paused) {
        QMetaObject::invokeMethod(this, [this, paused] {
            if (engine_.IsToolRunning())
                soundCoordinator_.playEvent(paused ? SoundEvent::RepeatPause : SoundEvent::RepeatResume);
        }, Qt::QueuedConnection);
    });
    rawInput_.setSelfInjectedFilter([this](const KeyDescriptor& key, bool pressed) {
        return inputDriver_.ConsumeSelfInjectedRaw(key, pressed);
    });
    rawInput_.setHookModifierSource([this](const KeyDescriptor& key) {
        return engine_.UsesHookModifierState(key);
    });
    rawInput_.registerWindow(reinterpret_cast<HWND>(winId()));
    for (const auto code : {VK_LCONTROL, VK_RCONTROL, VK_LSHIFT, VK_RSHIFT,
             VK_LMENU, VK_RMENU, VK_LWIN, VK_RWIN}) {
        const KeyDescriptor modifier{InputDevice::Keyboard, static_cast<std::uint32_t>(code)};
        const bool pressed = (GetAsyncKeyState(code) & 0x8000) != 0;
        inputDriver_.NotePhysicalKey(modifier, pressed);
        inputDriver_.NoteHookPhysicalKey(modifier, pressed);
    }
    inputHook_.setSelfInjectedFilter([this](const KeyDescriptor& key, bool pressed) {
        return inputDriver_.ConsumeSelfInjected(key, pressed);
    });
    inputHook_.setSelfInjectedWheelFilter([this](int delta) {
        return inputDriver_.ConsumeSelfInjectedWheel(delta);
    });
    inputHook_.setPhysicalInputFilter([this](const KeyDescriptor& key) {
        return inputDriver_.IsHeldDown(key);
    });
    inputHook_.setPhysicalKeyObserver([this](const KeyDescriptor& key, bool pressed) {
        inputDriver_.NoteHookPhysicalKey(key, pressed);
    });
    inputHook_.setPauseInputInterceptor([this](const KeyDescriptor& key, bool pressed, bool allowStart) {
        return engine_.InterceptPauseInput(key, pressed, allowStart);
    }, [this](int delta) {
        return engine_.InterceptPauseWheel(delta);
    }, [this](const KeyDescriptor& key) {
        return engine_.UsesHookModifierState(key);
    });

    engineHookCoordinator_.bind(EngineHookCoordinator::Context{
        &inputHook_,
        &rawInput_,
        [this](const KeyDescriptor& key, bool pressed) {
            inputDriver_.NotePhysicalKey(key, pressed);
        },
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
        [this]() { const auto* driver = pluginService_ ? pluginService_->activeDriver() : nullptr; return driver && driver->hardwareOutput(); },
        this,
        [this]() { return onboardingCoordinator_.blocksStart(); },
        [this](const RepeaterStatus& before, const RepeaterStatus& after) { soundCoordinator_.handleRepeatInput(before, after); }
    });

    if (!engineHookCoordinator_.start()) {
        throw std::runtime_error("failed to install global hooks");
    }
}
