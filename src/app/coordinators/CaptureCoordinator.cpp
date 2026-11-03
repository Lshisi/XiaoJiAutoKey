#include "coordinators/CaptureCoordinator.h"

#include "config/ActiveConfigValidator.h"
#include "controllers/MacroController.h"
#include "controllers/ProfileController.h"
#include "dialogs/AppMessageDialog.h"
#include "dialogs/KeyCaptureDialogBase.h"
#include "input/GlobalInputHook.h"
#include "input/InputKeyUtils.h"
#include "input/RawInputManager.h"
#include "ui/models/KeyPageModel.h"
#include "ui/models/MacroPageModel.h"
#include "ui/models/SettingsPageModel.h"

#include <algorithm>
#include <QMetaObject>
#include <QPointer>
#include <Windows.h>

CaptureCoordinator::CaptureCoordinator(QObject* parent) : QObject(parent) {}

void CaptureCoordinator::bind(Context context) {
    context_ = std::move(context);
    if (context_.rawInput) {
        connect(context_.rawInput, &RawInputManager::keyboardChanged, this, [this](const KeyDescriptor& key, bool pressed) {
            handleRawCaptureChanged(key, pressed);
        });
    }
    if (context_.inputHook) {
        connect(context_.inputHook, &GlobalInputHook::captureModifierChanged, this,
            [this](const KeyDescriptor& key, bool pressed) {
                if (!target_.has_value()) return;
                if (pressed) {
                    if (std::none_of(captureModifiers_.begin(), captureModifiers_.end(),
                        [&key](const KeyDescriptor& item) { return SamePrimaryKey(item, key); })) {
                        captureModifiers_.push_back(key);
                    }
                    if (*target_ == Target::DialogKey && activeDialog_) {
                        activeDialog_->handleCapturedKey(EncodeKey(key));
                    }
                    return;
                }

                std::erase_if(captureModifiers_, [&key](const KeyDescriptor& item) {
                    return SamePrimaryKey(item, key);
                });
                if (!captureModifiers_.empty()) return;
                if (*target_ == Target::DialogKey && activeDialog_) {
                    activeDialog_->handleModifierReleased(key);
                    target_.reset();
                    captureModifiers_.clear();
                    if (context_.refreshHookState) context_.refreshHookState();
                    return;
                }
                handleCapturedKey(key);
            });
    }
}

void CaptureCoordinator::handleRawCaptureChanged(const KeyDescriptor& key, bool pressed) {
    if (!target_.has_value()) return;

    if (IsKeyboardModifierKey(key)) {
        if (pressed) {
            if (std::none_of(captureModifiers_.begin(), captureModifiers_.end(),
                [&key](const KeyDescriptor& item) { return SamePrimaryKey(item, key); })) {
                captureModifiers_.push_back(key);
            }
            if (*target_ == Target::DialogKey && activeDialog_) {
                activeDialog_->handleCapturedKey(EncodeKey(key));
            }
            return;
        }

        std::erase_if(captureModifiers_, [&key](const KeyDescriptor& item) {
            return SamePrimaryKey(item, key);
        });
        if (!captureModifiers_.empty()) return;
        if (*target_ == Target::DialogKey && activeDialog_) {
            activeDialog_->handleModifierReleased(key);
            target_.reset();
            if (context_.refreshHookState) context_.refreshHookState();
            return;
        }
        handleCapturedKey(key);
        return;
    }

    if (!pressed) return;
    handleCapturedKey(key);
}

bool CaptureCoordinator::captureActive() const {
    return target_.has_value();
}

void CaptureCoordinator::startMainHotkeyCapture() {
    target_ = Target::MainHotkey;
    captureModifiers_.clear();
    if (context_.refreshHookState) context_.refreshHookState();
    if (context_.inputHook) context_.inputHook->setCaptureStartedTick(GetTickCount64());
    updateHotkeyText();
}

void CaptureCoordinator::startMacroHotkeyCapture() {
    target_ = Target::MacroHotkey;
    captureModifiers_.clear();
    if (context_.refreshHookState) context_.refreshHookState();
    if (context_.inputHook) context_.inputHook->setCaptureStartedTick(GetTickCount64());
    if (context_.macroPage) {
        context_.macroPage->setHotkeyText(QString::fromWCharArray(L"\u5f55\u5236\u4e2d"));
    }
}

void CaptureCoordinator::runKeyCaptureDialog(KeyCaptureDialogBase& dialog) {
    if (!context_.inputHook) return;

    auto keyConn = connect(context_.inputHook, &GlobalInputHook::capturedKey, &dialog, [&](quint64 encodedKey) {
        if (!target_.has_value() || *target_ != Target::DialogKey) {
            return;
        }
        dialog.handleCapturedKey(encodedKey);
        target_.reset();
        captureModifiers_.clear();
        if (context_.refreshHookState) context_.refreshHookState();
    });
    auto requestConn = connect(&dialog, &KeyCaptureDialogBase::keyCaptureRequested, this, [this, &dialog]() {
        activeDialog_ = &dialog;
        target_ = Target::DialogKey;
        captureModifiers_.clear();
        if (context_.refreshHookState) context_.refreshHookState();
        if (context_.inputHook) context_.inputHook->setCaptureStartedTick(GetTickCount64());
    });

    context_.inputHook->setDialogOpen(true);
    dialog.exec();
    context_.inputHook->setDialogOpen(false);

    disconnect(keyConn);
    disconnect(requestConn);
    activeDialog_ = nullptr;
    target_.reset();
    captureModifiers_.clear();
    if (context_.refreshHookState) context_.refreshHookState();
    updateHotkeyText();
}

void CaptureCoordinator::handleCapturedKey(quint64 encodedKey) {
    handleCapturedKey(DecodeKey(encodedKey));
}

void CaptureCoordinator::handleCapturedKey(const KeyDescriptor& key) {
    captureModifiers_.clear();
    handleDecodedKey(key);
}

void CaptureCoordinator::cancelCapture() {
    if (!target_.has_value()) return;
    target_.reset();
    captureModifiers_.clear();
    if (context_.refreshHookState) context_.refreshHookState();
    updateHotkeyText();
}

void CaptureCoordinator::handleDecodedKey(const KeyDescriptor& key) {
    if (!target_.has_value()) return;

    if (*target_ == Target::DialogKey && activeDialog_) {
        activeDialog_->handleCapturedKey(EncodeKey(key));
        target_.reset();
        captureModifiers_.clear();
        if (context_.refreshHookState) context_.refreshHookState();
        return;
    }

    if (*target_ == Target::MainHotkey) {
        target_.reset();
        if (context_.refreshHookState) context_.refreshHookState();
        if (!context_.profileController) return;

        if (context_.validateHotkeyCandidate) {
            auto validation = context_.validateHotkeyCandidate(key);
            if (!validation.ok) {
                AppMessageDialog::warning(context_.parent, QString::fromWCharArray(L"\u6309\u952e\u51b2\u7a81"), validation.message);
                updateHotkeyText();
                if (context_.refreshAll) context_.refreshAll();
                return;
            }
        }

        auto result = context_.profileController->setControlHotkey(key);
        if (!result.ok) {
            AppMessageDialog::warning(context_.parent, QString::fromWCharArray(L"\u6309\u952e\u51b2\u7a81"), result.message);
        } else {
            if (context_.saveConfig) context_.saveConfig();
            if (context_.refreshHookState) context_.refreshHookState();
        }
        updateHotkeyText();
        if (context_.refreshAll) context_.refreshAll();
        return;
    }

    if (*target_ == Target::MacroHotkey) {
        target_.reset();
        if (context_.refreshHookState) context_.refreshHookState();
        if (!context_.macroController) return;

        if (context_.validateHotkeyCandidate) {
            auto validation = context_.validateHotkeyCandidate(key);
            if (!validation.ok) {
                AppMessageDialog::warning(context_.parent, QString::fromWCharArray(L"\u6309\u952e\u51b2\u7a81"), validation.message);
                if (context_.refreshMacroPage) context_.refreshMacroPage();
                return;
            }
        }

        auto result = context_.macroController->setTriggerKey(key);
        if (!result.ok) {
            AppMessageDialog::warning(context_.parent, QString::fromWCharArray(L"\u6309\u952e\u51b2\u7a81"), result.message);
        } else {
            if (context_.applyMacros) context_.applyMacros();
            if (context_.saveConfig) context_.saveConfig();
        }
        if (context_.refreshMacroPage) context_.refreshMacroPage();
        return;
    }

    if (*target_ == Target::DialogKey && activeDialog_) return;
    target_.reset();
    if (context_.refreshHookState) context_.refreshHookState();
}

void CaptureCoordinator::updateHotkeyText() {
    if (!context_.keyPage || !context_.settingsPage || !context_.profileController) return;
    auto* profile = context_.profileController->activeProfile();
    const QString text = target_.has_value() && *target_ == Target::MainHotkey
        ? QString::fromWCharArray(L"\u5f55\u5236\u4e2d")
        : (profile && profile->controlHotkey.has_value()
            ? QString::fromStdWString(GetKeyDisplayName(*profile->controlHotkey))
            : QString::fromWCharArray(L"\u672a\u8bbe\u7f6e"));
    context_.keyPage->setHotkeyText(text);
    context_.keyPage->setSequenceSwitchKey(profile ? profile->sequenceSwitchKey : std::nullopt);
    context_.settingsPage->setHotkeyText(text);
}
