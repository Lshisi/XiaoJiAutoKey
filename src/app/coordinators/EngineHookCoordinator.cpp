#include "coordinators/EngineHookCoordinator.h"

#include "input/GlobalInputHook.h"
#include "dialogs/AppMessageDialog.h"
#include "input/RawInputManager.h"
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
        handleControlHotkeyPressed();
    });
    connect(context_.hook, &GlobalInputHook::physicalKeyChanged, this, [this](const KeyDescriptor& key, bool pressed) {
        handlePhysicalChanged(key, pressed);
    });
    connect(context_.hook, &GlobalInputHook::macroTriggerChanged, this, [this](const KeyDescriptor& key, bool pressed) {
        handleMacroTriggerChanged(key, pressed);
    });

    if (context_.rawInput) {
        connect(context_.rawInput, &RawInputManager::keyboardChanged, this, [this](const KeyDescriptor& key, bool pressed) {
            handleRawInputChanged(key, pressed);
        });
        connect(context_.rawInput, &RawInputManager::mouseChanged, this, [this](const KeyDescriptor& key, bool pressed) {
            handleRawInputChanged(key, pressed);
        });
    }
}

bool EngineHookCoordinator::start() {
    if (!context_.hook) return false;
    syncInputModes();
    return context_.hook->start();
}

void EngineHookCoordinator::refreshState() {
    if (!context_.hook) return;
    context_.hook->setControlHotkey(context_.controlHotkey ? context_.controlHotkey() : std::nullopt);
    context_.hook->setCaptureActive(context_.captureActive ? context_.captureActive() : false);
    context_.hook->setRepeatRunning(context_.repeatRunning ? context_.repeatRunning() : false);
    syncInputModes();
    rawControlHotkeyPressed_ = false;
}

void EngineHookCoordinator::syncInputModes() {
    if (!context_.hook) return;

    const bool rawKeyboardActive = context_.rawInput && context_.rawInput->isActive();
    const bool rawMouseActive = context_.rawInput && context_.rawInput->isMouseActive();
    context_.hook->setRawKeyboardActive(rawKeyboardActive);
    context_.hook->setRawMouseActive(rawMouseActive);
    if (context_.repeaterEngine) {
        context_.repeaterEngine->SetSelfInjectedKeyboardVisible(false);
        const bool selfInjectedMouseVisible = !rawMouseActive &&
            (!context_.selfInjectedMouseVisible || context_.selfInjectedMouseVisible());
        context_.repeaterEngine->SetSelfInjectedMouseVisible(selfInjectedMouseVisible);
    }
}

void EngineHookCoordinator::handleRawInputChanged(const KeyDescriptor& key, bool pressed) {
    if (context_.captureActive && context_.captureActive()) {
        return;
    }
    if (context_.hook && context_.hook->isDialogOpen()) {
        return;
    }

    const auto controlHotkey = context_.controlHotkey ? context_.controlHotkey() : std::nullopt;
    const bool hookRunning = context_.hook && context_.hook->isRunning();
    if (controlHotkey.has_value() && *controlHotkey == key) {
        // The low-level hook owns this key while it is alive. Raw Input only
        // takes over when the hook failed or has already exited.
        if (pressed && !rawControlHotkeyPressed_) {
            rawControlHotkeyPressed_ = true;
            if (!hookRunning) {
                handleControlHotkeyPressed();
            }
        } else if (!pressed) {
            rawControlHotkeyPressed_ = false;
        }
    } else {
        handlePhysicalChanged(key, pressed);
    }

    if (!hookRunning ||
        !controlHotkey.has_value() || *controlHotkey != key) {
        handleMacroTriggerChanged(key, pressed);
    }
}

void EngineHookCoordinator::handlePhysicalChanged(const KeyDescriptor& key, bool pressed) {
    if (!context_.repeaterEngine) return;
    if (context_.repeatRunning && !context_.repeatRunning()) return;
    if (pressed) context_.repeaterEngine->OnPhysicalDown(key);
    else context_.repeaterEngine->OnPhysicalUp(key);
}

void EngineHookCoordinator::handleControlHotkeyPressed() {
    if (context_.canUseDdDriver && !context_.canUseDdDriver()) {
        if (context_.showDdUnavailableMessage) context_.showDdUnavailableMessage();
        return;
    }
    if (context_.repeaterEngine && !context_.repeaterEngine->IsToolRunning() && context_.validateActiveConfig) {
        auto validation = context_.validateActiveConfig();
        if (!validation.ok) {
            if (!conflictPromptVisible_) {
                conflictPromptVisible_ = true;
                AppMessageDialog::warning(context_.parent, QString::fromWCharArray(L"\u6309\u952e\u51b2\u7a81"), validation.message);
                conflictPromptVisible_ = false;
            }
            return;
        }
    }
    if (context_.toggleRepeatRunning) context_.toggleRepeatRunning();
}

void EngineHookCoordinator::handleMacroTriggerChanged(const KeyDescriptor& key, bool pressed) {
    if (!context_.macroEngine) return;
    if (pressed) {
        if (context_.canUseDdDriver && !context_.canUseDdDriver()) {
            if (context_.showDdUnavailableMessage) context_.showDdUnavailableMessage();
            return;
        }
        const auto triggerKey = context_.macroTriggerKey ? context_.macroTriggerKey() : std::nullopt;
        if (triggerKey.has_value() && *triggerKey == key
            && !context_.macroEngine->HasActiveMacro()
            && context_.validateActiveConfig) {
            auto validation = context_.validateActiveConfig();
            if (!validation.ok) {
                if (!conflictPromptVisible_) {
                    conflictPromptVisible_ = true;
                    AppMessageDialog::warning(context_.parent, QString::fromWCharArray(L"\u6309\u952e\u51b2\u7a81"), validation.message);
                    conflictPromptVisible_ = false;
                }
                return;
            }
        }
        context_.macroEngine->OnTriggerDown(key);
        if (context_.macroStateChanged) context_.macroStateChanged();
    }
    else {
        context_.macroEngine->OnTriggerUp(key);
        if (context_.macroStateChanged) context_.macroStateChanged();
    }
}
