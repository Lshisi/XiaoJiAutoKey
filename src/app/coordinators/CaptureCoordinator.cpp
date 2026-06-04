#include "coordinators/CaptureCoordinator.h"

#include "controllers/MacroController.h"
#include "controllers/ProfileController.h"
#include "dialogs/KeyCaptureDialogBase.h"
#include "input/GlobalInputHook.h"
#include "input/InputKeyUtils.h"
#include "ui/pages/KeyPage.h"
#include "ui/pages/MacroPage.h"
#include "ui/pages/SettingsPage.h"

#include <QMessageBox>
#include <Windows.h>

CaptureCoordinator::CaptureCoordinator(QObject* parent) : QObject(parent) {}

void CaptureCoordinator::bind(Context context) {
    context_ = std::move(context);
}

bool CaptureCoordinator::captureActive() const {
    return target_.has_value();
}

void CaptureCoordinator::startMainHotkeyCapture() {
    target_ = Target::MainHotkey;
    if (context_.refreshHookState) context_.refreshHookState();
    if (context_.inputHook) context_.inputHook->setCaptureStartedTick(GetTickCount64());
    updateHotkeyText();
}

void CaptureCoordinator::startMacroHotkeyCapture() {
    target_ = Target::MacroHotkey;
    if (context_.refreshHookState) context_.refreshHookState();
    if (context_.inputHook) context_.inputHook->setCaptureStartedTick(GetTickCount64());
    if (context_.macroPage) {
        context_.macroPage->setHotkeyText(QString::fromWCharArray(L"\u8bf7\u6309\u4e0b\u4e00\u4e2a\u952e..."));
    }
}

void CaptureCoordinator::runKeyCaptureDialog(KeyCaptureDialogBase& dialog) {
    if (!context_.inputHook) return;

    auto keyConn = connect(context_.inputHook, &GlobalInputHook::capturedKey, &dialog, [&](quint64 encodedKey) {
        dialog.handleCapturedKey(encodedKey);
        target_.reset();
        if (context_.refreshHookState) context_.refreshHookState();
    });
    auto requestConn = connect(&dialog, &KeyCaptureDialogBase::keyCaptureRequested, this, [this, &dialog]() {
        activeDialog_ = &dialog;
        target_ = Target::DialogKey;
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
    if (context_.refreshHookState) context_.refreshHookState();
    updateHotkeyText();
}

void CaptureCoordinator::handleCapturedKey(quint64 encodedKey) {
    handleDecodedKey(DecodeKey(encodedKey));
}

void CaptureCoordinator::cancelCapture() {
    if (!target_.has_value()) return;
    target_.reset();
    if (context_.refreshHookState) context_.refreshHookState();
    updateHotkeyText();
}

void CaptureCoordinator::handleDecodedKey(const KeyDescriptor& key) {
    if (!target_.has_value()) return;

    if (*target_ == Target::MainHotkey) {
        target_.reset();
        if (context_.refreshHookState) context_.refreshHookState();
        if (!context_.profileController) return;

        auto result = context_.profileController->setControlHotkey(key);
        if (!result.ok) {
            QMessageBox::warning(context_.parent, QString::fromWCharArray(L"\u6309\u952e\u51b2\u7a81"), result.message);
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

        auto result = context_.macroController->setTriggerKey(key);
        if (!result.ok) {
            QMessageBox::warning(context_.parent, QString::fromWCharArray(L"\u6309\u952e\u51b2\u7a81"), result.message);
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
        ? QString::fromWCharArray(L"\u8bf7\u6309\u4e0b\u4e00\u4e2a\u952e...")
        : (profile && profile->controlHotkey.has_value()
            ? QString::fromStdWString(GetKeyDisplayName(*profile->controlHotkey))
            : QString::fromWCharArray(L"\u672a\u8bbe\u7f6e"));
    context_.keyPage->setHotkeyText(text);
    context_.settingsPage->setHotkeyText(text);
}
