#include "coordinators/KeyBindingsCoordinator.h"

#include "config/ActiveConfigValidator.h"
#include "controllers/ProfileController.h"
#include "dialogs/AppMessageDialog.h"
#include "dialogs/BindingEditDialog.h"
#include "dialogs/PauseKeyDialog.h"
#include "RepeaterEngine.h"
#include "ui/pages/KeyPage.h"

#include <QDialog>

KeyBindingsCoordinator::KeyBindingsCoordinator(QObject* parent) : QObject(parent) {}

void KeyBindingsCoordinator::bind(Context context) {
    context_ = std::move(context);
    if (!context_.page) return;

    connect(context_.page, &KeyPage::addRequested, this, &KeyBindingsCoordinator::onAddBinding);
    connect(context_.page, &KeyPage::editRequested, this, &KeyBindingsCoordinator::onEditBinding);
    connect(context_.page, &KeyPage::deleteRequested, this, &KeyBindingsCoordinator::onDeleteBinding);
    connect(context_.page, &KeyPage::enableToggled, this, &KeyBindingsCoordinator::onEnableToggled);
    connect(context_.page, &KeyPage::startStopClicked, this, &KeyBindingsCoordinator::onStartClicked);
    connect(context_.page, &KeyPage::hotkeyEditClicked, this, &KeyBindingsCoordinator::onHotkeyEditClicked);
    connect(context_.page, &KeyPage::hotkeyClearClicked, this, &KeyBindingsCoordinator::onHotkeyClearClicked);
    connect(context_.page, &KeyPage::beepToggled, this, &KeyBindingsCoordinator::onBeepToggled);
    connect(context_.page, &KeyPage::statusOverlayToggled, this, &KeyBindingsCoordinator::onStatusOverlayToggled);
    connect(context_.page, &KeyPage::mixedModeToggled, this, &KeyBindingsCoordinator::onMixedModeToggled);
    connect(context_.page, &KeyPage::pauseKeyAddClicked, this, &KeyBindingsCoordinator::onPauseKeyAdd);
    connect(context_.page, &KeyPage::pauseKeyDeleteClicked, this, &KeyBindingsCoordinator::onPauseKeyDelete);
}

void KeyBindingsCoordinator::onAddBinding() {
    openEditDialog(std::nullopt);
}

void KeyBindingsCoordinator::onEditBinding(std::uint64_t id) {
    openEditDialog(id);
}

void KeyBindingsCoordinator::onDeleteBinding(std::uint64_t id) {
    if (!context_.controller) return;
    auto result = context_.controller->deleteBinding(id);
    if (!result.ok) return;
    auto* profile = context_.controller->activeProfile();
    if (!profile) return;

    context_.page->setBindings(profile->bindings);
    if (context_.applyBindings) context_.applyBindings();
    if (context_.saveConfig) context_.saveConfig();
    if (context_.refreshAll) context_.refreshAll();
}

void KeyBindingsCoordinator::onEnableToggled(std::uint64_t id, bool enabled) {
    if (!context_.controller) return;
    if (enabled) {
        auto validation = ActiveConfigValidator::validateActiveHotkeys(
            context_.controller->activeProfile(),
            context_.activeMacro ? context_.activeMacro() : nullptr);
        if (!validation.ok) {
            AppMessageDialog::warning(context_.parent, QString::fromWCharArray(L"\u6309\u952e\u51b2\u7a81"), validation.message);
            if (context_.refreshAll) context_.refreshAll();
            return;
        }
    }
    auto result = context_.controller->setBindingEnabled(id, enabled);
    if (!result.ok) return;
    if (context_.applyBindings) context_.applyBindings();
    if (context_.saveConfig) context_.saveConfig();
}

void KeyBindingsCoordinator::onStartClicked() {
    if (!context_.engine || !context_.setRepeatRunning) return;
    const bool shouldStart = !context_.engine->IsToolRunning();
    if (shouldStart && context_.controller) {
        auto validation = ActiveConfigValidator::validateActiveHotkeys(
            context_.controller->activeProfile(),
            context_.activeMacro ? context_.activeMacro() : nullptr);
        if (!validation.ok) {
            AppMessageDialog::warning(context_.parent, QString::fromWCharArray(L"\u6309\u952e\u51b2\u7a81"), validation.message);
            return;
        }
    }
    context_.setRepeatRunning(shouldStart);
}

void KeyBindingsCoordinator::onHotkeyEditClicked() {
    if (context_.startHotkeyCapture) context_.startHotkeyCapture();
}

void KeyBindingsCoordinator::onHotkeyClearClicked() {
    if (!context_.controller) return;
    auto result = context_.controller->clearControlHotkey();
    if (!result.ok) return;
    if (context_.saveConfig) context_.saveConfig();
    if (context_.applyBindings) context_.applyBindings();
    if (context_.refreshAll) context_.refreshAll();
}

void KeyBindingsCoordinator::onBeepToggled(bool enabled) {
    if (!context_.page) return;
    if (context_.setBeepEnabled) context_.setBeepEnabled(enabled);
    context_.page->setBeepEnabled(enabled);
    if (context_.saveConfig) context_.saveConfig();
}

void KeyBindingsCoordinator::onStatusOverlayToggled(bool enabled) {
    if (!context_.page) return;
    if (context_.setStatusOverlayEnabled) context_.setStatusOverlayEnabled(enabled);
    context_.page->setStatusOverlayEnabled(enabled);
    if (context_.saveConfig) context_.saveConfig();
    if (context_.refreshAll) context_.refreshAll();
}

void KeyBindingsCoordinator::onMixedModeToggled(bool enabled) {
    if (!context_.controller || !context_.engine) return;
    auto result = context_.controller->setMixedMode(enabled);
    if (!result.ok) return;
    context_.engine->SetMixedMode(enabled);
    if (context_.saveConfig) context_.saveConfig();
}

void KeyBindingsCoordinator::onPauseKeyAdd() {
    if (!context_.controller || !context_.runKeyCaptureDialog) return;
    auto* profile = context_.controller->activeProfile();
    if (!profile) return;

    PauseKeyDialog dialog(context_.parent);
    context_.runKeyCaptureDialog(dialog);
    if (dialog.result() == QDialog::Rejected) return;

    auto validation = ActiveConfigValidator::validateChildKeyCandidate(
        dialog.editResult().key,
        profile,
        context_.activeMacro ? context_.activeMacro() : nullptr);
    if (!validation.ok) {
        AppMessageDialog::warning(context_.parent, QString::fromWCharArray(L"\u6309\u952e\u51b2\u7a81"), validation.message);
        return;
    }

    auto result = context_.controller->addPauseKey(dialog.editResult().key);
    if (!result.ok) {
        AppMessageDialog::warning(context_.parent, QString::fromWCharArray(L"\u6309\u952e\u51b2\u7a81"), result.message);
        return;
    }
    if (context_.applyBindings) context_.applyBindings();
    if (context_.saveConfig) context_.saveConfig();
    context_.page->setPauseKeys(profile->pauseKeys);
}

void KeyBindingsCoordinator::onPauseKeyDelete(int index) {
    if (!context_.controller) return;
    auto result = context_.controller->deletePauseKey(index);
    if (!result.ok) return;
    auto* profile = context_.controller->activeProfile();
    if (!profile) return;
    if (context_.applyBindings) context_.applyBindings();
    if (context_.saveConfig) context_.saveConfig();
    context_.page->setPauseKeys(profile->pauseKeys);
}

void KeyBindingsCoordinator::openEditDialog(std::optional<std::uint64_t> bindingId) {
    if (!context_.controller || !context_.runKeyCaptureDialog) return;
    auto* profile = context_.controller->activeProfile();
    if (!profile) return;

    std::optional<BindingEditResult> existing;
    if (bindingId.has_value()) {
        if (auto idx = findBindingIndex(*bindingId)) {
            const auto& binding = profile->bindings[*idx];
            existing = BindingEditResult{binding.key, binding.mode, binding.delayMs, binding.enabled, binding.suppressMixedMode};
        }
    }

    BindingEditDialog dialog(existing, profile->defaultDelayMs, context_.parent);
    context_.runKeyCaptureDialog(dialog);
    if (dialog.result() == QDialog::Rejected) return;

    const auto editResult = dialog.editResult();
    auto validation = ActiveConfigValidator::validateChildKeyCandidate(
        editResult.key,
        profile,
        context_.activeMacro ? context_.activeMacro() : nullptr);
    if (!validation.ok) {
        AppMessageDialog::warning(context_.parent, QString::fromWCharArray(L"\u6309\u952e\u51b2\u7a81"), validation.message);
        return;
    }

    auto result = context_.controller->upsertBinding(
        bindingId,
        editResult.key,
        editResult.mode,
        editResult.delayMs,
        editResult.enabled,
        editResult.suppressMixedMode);
    if (!result.ok) {
        AppMessageDialog::warning(context_.parent, QString::fromWCharArray(L"\u6309\u952e\u51b2\u7a81"), result.message);
        return;
    }

    context_.page->setBindings(profile->bindings);
    if (context_.applyBindings) context_.applyBindings();
    if (context_.saveConfig) context_.saveConfig();
    if (context_.refreshAll) context_.refreshAll();
}

std::optional<std::size_t> KeyBindingsCoordinator::findBindingIndex(std::uint64_t id) const {
    if (!context_.controller) return std::nullopt;
    auto* profile = context_.controller->activeProfile();
    if (!profile) return std::nullopt;
    for (std::size_t i = 0; i < profile->bindings.size(); ++i) {
        if (profile->bindings[i].id == id) return i;
    }
    return std::nullopt;
}
