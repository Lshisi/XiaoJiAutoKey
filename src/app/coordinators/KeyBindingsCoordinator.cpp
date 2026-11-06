#include "coordinators/KeyBindingsCoordinator.h"

#include "config/ActiveConfigValidator.h"
#include "controllers/ProfileController.h"
#include "dialogs/AppMessageDialog.h"
#include "dialogs/BatchEditDialog.h"
#include "dialogs/BindingEditDialog.h"
#include "dialogs/ConfigConfirmDialog.h"
#include "dialogs/PauseKeyDialog.h"
#include "dialogs/SequenceSwitchKeyDialog.h"
#include "input/GlobalInputHook.h"
#include "RepeaterEngine.h"
#include "ui/models/KeyPageModel.h"
#include "ui/models/KeyOverviewData.h"

#include <QDialog>
#include <QScopedValueRollback>

KeyBindingsCoordinator::KeyBindingsCoordinator(QObject* parent) : QObject(parent) {}

void KeyBindingsCoordinator::bind(Context context) {
    context_ = std::move(context);
    if (!context_.page) return;

    connect(context_.page, &KeyPageModel::addRequested, this, &KeyBindingsCoordinator::onAddBinding);
    connect(context_.page, &KeyPageModel::editRequested, this, &KeyBindingsCoordinator::onEditBinding);
    connect(context_.page, &KeyPageModel::deleteRequested, this, &KeyBindingsCoordinator::onDeleteBinding);
    connect(context_.page, &KeyPageModel::enableToggled, this, &KeyBindingsCoordinator::onEnableToggled);
    connect(context_.page, &KeyPageModel::batchEditRequestedImpl, this, &KeyBindingsCoordinator::onBatchEdit);
    connect(context_.page, &KeyPageModel::batchDeleteRequestedImpl, this, &KeyBindingsCoordinator::onBatchDelete);
    connect(context_.page, &KeyPageModel::startStopClicked, this, &KeyBindingsCoordinator::onStartClicked);
    connect(context_.page, &KeyPageModel::hotkeyEditClicked, this, &KeyBindingsCoordinator::onHotkeyEditClicked);
    connect(context_.page, &KeyPageModel::hotkeyClearClicked, this, &KeyBindingsCoordinator::onHotkeyClearClicked);
    connect(context_.page, &KeyPageModel::beepToggled, this, &KeyBindingsCoordinator::onBeepToggled);
    connect(context_.page, &KeyPageModel::statusOverlayToggled, this, &KeyBindingsCoordinator::onStatusOverlayToggled);
    connect(context_.page, &KeyPageModel::mixedModeToggled, this, &KeyBindingsCoordinator::onMixedModeToggled);
    connect(context_.page, &KeyPageModel::wasdProtectionToggled, this, &KeyBindingsCoordinator::onWasdProtectionToggled);
    connect(context_.page, &KeyPageModel::rememberSequencePositionChanged, this, &KeyBindingsCoordinator::onRememberSequencePositionChanged);
    connect(context_.page, &KeyPageModel::sequenceSwitchKeyAddClicked, this, &KeyBindingsCoordinator::onSequenceSwitchKeyAdd);
    connect(context_.page, &KeyPageModel::sequenceSwitchKeyDeleteClicked, this, &KeyBindingsCoordinator::onSequenceSwitchKeyDelete);
    connect(context_.page, &KeyPageModel::pauseKeyAddClicked, this, &KeyBindingsCoordinator::onPauseKeyAdd);
    connect(context_.page, &KeyPageModel::pauseKeyEditClicked, this, [this](int index) { editPauseKey(index); });
    connect(context_.page, &KeyPageModel::pauseKeyDeleteClicked, this, &KeyBindingsCoordinator::onPauseKeyDelete);
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

    context_.page->setBindings(profile->bindings, profile->id);
    if (context_.applyBindings) context_.applyBindings();
    if (context_.saveConfig) context_.saveConfig();
    if (context_.refreshAll) context_.refreshAll();
}

void KeyBindingsCoordinator::onEnableToggled(std::uint64_t id, bool enabled) {
    if (!context_.controller) return;
    if (enabled) {
        auto* profile = context_.controller->activeProfile();
        const auto index = findBindingIndex(id);
        if (!profile || !index.has_value()) return;

        auto validation = ActiveConfigValidator::validateActiveHotkeys(
            profile,
            context_.activeMacro ? context_.activeMacro() : nullptr);
        if (validation.ok) {
            validation = ActiveConfigValidator::validateRepeatBindingCandidate(
                profile->bindings[*index].key,
                true,
                profile,
                context_.activeMacro ? context_.activeMacro() : nullptr);
        }
        if (!validation.ok) {
            AppMessageDialog::warning(context_.parent, QString::fromWCharArray(L"\u6309\u952e\u51b2\u7a81"), validation.message);
            if (context_.refreshAll) context_.refreshAll();
            return;
        }
    }
    auto result = context_.controller->setBindingEnabled(id, enabled);
    if (!result.ok) {
        AppMessageDialog::warning(context_.parent, QString::fromWCharArray(L"\u6309\u952e\u51b2\u7a81"), result.message);
        if (context_.refreshAll) context_.refreshAll();
        return;
    }
    if (context_.applyBindings) context_.applyBindings();
    if (context_.saveConfig) context_.saveConfig();
    auto* profile = context_.controller->activeProfile();
    if (!profile) return;
    context_.page->setBindings(profile->bindings, profile->id);
}

void KeyBindingsCoordinator::onStartClicked() {
    if (!context_.engine || !context_.setRepeatRunning) return;
    const bool shouldStart = !context_.engine->IsToolRunning();
    if (shouldStart && context_.controller) {
        const auto* profile = context_.controller->activeProfile();
        if (!profile || !profile->enabled) return;
    }
    if (shouldStart && context_.canUseDdDriver && !context_.canUseDdDriver()) {
        if (context_.showDdUnavailableMessage) context_.showDdUnavailableMessage();
        return;
    }
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

void KeyBindingsCoordinator::onBatchEdit(const QVariantList& bindingIds) {
    if (!context_.controller || bindingIds.isEmpty()) return;

    std::vector<std::uint64_t> ids;
    ids.reserve(bindingIds.size());
    for (const auto& value : bindingIds) {
        bool ok = false;
        const std::uint64_t id = value.toString().toULongLong(&ok);
        if (ok) ids.push_back(id);
    }
    if (ids.empty()) return;

    if (context_.inputHook) context_.inputHook->setDialogOpen(true);
    BatchEditDialog dialog(false, static_cast<int>(ids.size()), context_.parent);
    const int result = dialog.exec();
    if (context_.inputHook) context_.inputHook->setDialogOpen(false);
    if (result != QDialog::Accepted) return;

    auto editResult = context_.controller->updateBindings(
        ids,
        context_.activeMacro ? context_.activeMacro() : nullptr,
        dialog.mode(),
        dialog.delayMs(),
        dialog.enabled(),
        dialog.participatePriority());
    if (!editResult.ok) {
        AppMessageDialog::warning(context_.parent, QString::fromWCharArray(L"\u6279\u91cf\u4fee\u6539\u5931\u8d25"), editResult.message);
        if (context_.refreshAll) context_.refreshAll();
        return;
    }

    auto* profile = context_.controller->activeProfile();
    if (!profile) return;
    context_.page->setBindings(profile->bindings, profile->id);
    if (context_.applyBindings) context_.applyBindings();
    if (context_.saveConfig) context_.saveConfig();
    if (context_.refreshAll) context_.refreshAll();
}

void KeyBindingsCoordinator::onHotkeyEditClicked() {
    if (context_.startHotkeyCapture) context_.startHotkeyCapture();
}

void KeyBindingsCoordinator::onBatchDelete(const QVariantList& bindingIds) {
    if (!context_.controller || !context_.inputHook || bindingIds.isEmpty()) return;
    if (context_.engine && context_.engine->IsToolRunning()) return;
    std::vector<std::uint64_t> ids;
    for (const auto& value : bindingIds) ids.push_back(value.toString().toULongLong());

    context_.inputHook->setDialogOpen(true);
    const bool confirmed = ConfigConfirmDialog::confirm(context_.parent, tr("批量删除按键"),
        tr("确定要删除选中的 %1 个按键吗？").arg(ids.size()), tr("删除"));
    context_.inputHook->setDialogOpen(false);
    if (!confirmed) return;

    const auto result = context_.controller->deleteBindings(ids);
    if (!result.ok) return;
    if (context_.applyBindings) context_.applyBindings();
    if (context_.saveConfig) context_.saveConfig();
    if (context_.refreshAll) context_.refreshAll();
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
    context_.page->setMixedMode(enabled);
    if (context_.saveConfig) context_.saveConfig();
}

void KeyBindingsCoordinator::onWasdProtectionToggled(bool enabled) {
    if (!context_.controller || !context_.engine) return;
    auto result = context_.controller->setWasdProtection(enabled);
    if (!result.ok) return;
    context_.engine->SetWasdProtection(enabled);
    context_.page->setWasdProtection(enabled);
    if (context_.saveConfig) context_.saveConfig();
}

void KeyBindingsCoordinator::onRememberSequencePositionChanged(bool enabled) {
    if (!context_.controller || !context_.engine) return;
    if (context_.engine->IsToolRunning()) {
        if (context_.refreshAll) context_.refreshAll();
        return;
    }
    const auto result = context_.controller->setRememberSequencePosition(enabled);
    if (!result.ok) return;
    context_.page->setRememberSequencePosition(enabled);
    if (context_.applyBindings) context_.applyBindings();
    if (context_.saveConfig) context_.saveConfig();
}

void KeyBindingsCoordinator::onSequenceSwitchKeyAdd() {
    if (!context_.controller || !context_.runKeyCaptureDialog) return;
    auto* profile = context_.controller->activeProfile();
    if (!profile) return;

    SequenceSwitchKeyDialog dialog(context_.parent, profile->sequenceSwitchKey);
    context_.runKeyCaptureDialog(dialog);
    if (dialog.result() == QDialog::Rejected) return;

    auto validation = ActiveConfigValidator::validateSequenceSwitchKeyCandidate(
        dialog.editResult().key,
        profile,
        context_.activeMacro ? context_.activeMacro() : nullptr);
    if (!validation.ok) {
        AppMessageDialog::warning(context_.parent, QString::fromWCharArray(L"\u6309\u952e\u51b2\u7a81"), validation.message);
        return;
    }

    auto result = context_.controller->setSequenceSwitchKey(dialog.editResult().key);
    if (!result.ok) {
        AppMessageDialog::warning(context_.parent, QString::fromWCharArray(L"\u6309\u952e\u51b2\u7a81"), result.message);
        return;
    }
    if (context_.applyBindings) context_.applyBindings();
    if (context_.saveConfig) context_.saveConfig();
    if (context_.refreshAll) context_.refreshAll();
}

void KeyBindingsCoordinator::onSequenceSwitchKeyDelete() {
    if (!context_.controller) return;
    auto result = context_.controller->clearSequenceSwitchKey();
    if (!result.ok) return;
    if (context_.applyBindings) context_.applyBindings();
    if (context_.saveConfig) context_.saveConfig();
    if (context_.refreshAll) context_.refreshAll();
}

void KeyBindingsCoordinator::onPauseKeyAdd() {
    editPauseKey(std::nullopt);
}

void KeyBindingsCoordinator::editPauseKey(std::optional<int> index) {
    if (!context_.controller || !context_.runKeyCaptureDialog) return;
    if (context_.engine && context_.engine->IsToolRunning()) return;
    auto* profile = context_.controller->activeProfile();
    if (!profile) return;
    if (index && (*index < 0 || *index >= static_cast<int>(profile->pauseKeys.size()))) return;

    PauseKeyDialog dialog(context_.parent, index ? std::optional(profile->pauseKeys[*index]) : std::nullopt);
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

    auto result = context_.controller->upsertPauseKey(index, dialog.editResult().key);
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

void KeyBindingsCoordinator::editFromOverview(const QVariantMap& target, QWidget* parent) {
    QScopedValueRollback<QWidget*> parentScope(context_.parent, parent);
    const QString action = target.value("action").toString();
    if (action == QStringLiteral("add")) {
        const auto key = KeyOverviewData::descriptor(target);
        if (key.IsValid()) openEditDialog(std::nullopt, key);
    } else if (action == QStringLiteral("output")) onEditBinding(target.value("targetId").toString().toULongLong());
    else if (action == QStringLiteral("sequence")) onSequenceSwitchKeyAdd();
    else if (action == QStringLiteral("pause")) editPauseKey(target.value("targetId").toInt());
    else if (action == QStringLiteral("hotkey")) onHotkeyEditClicked();
}

void KeyBindingsCoordinator::openEditDialog(std::optional<std::uint64_t> bindingId, std::optional<KeyDescriptor> initialKey) {
    if (!context_.controller || !context_.runKeyCaptureDialog) return;
    if (context_.engine && context_.engine->IsToolRunning()) return;
    auto* profile = context_.controller->activeProfile();
    if (!profile) return;

    std::optional<BindingEditResult> existing;
    if (bindingId.has_value()) {
        if (auto idx = findBindingIndex(*bindingId)) {
            const auto& binding = profile->bindings[*idx];
            existing = BindingEditResult{
                binding.key, QString::fromStdWString(binding.name), binding.mode, binding.delayMs, binding.enabled,
                binding.suppressMixedMode, binding.clickPos};
        }
    }

    BindingEditDialog dialog(existing, kDefaultDelayMs,
        context_.driverSupportsMouseMove, context_.parent);
    if (!existing && initialKey) dialog.setInitialKey(*initialKey);
    context_.runKeyCaptureDialog(dialog);
    if (dialog.result() == QDialog::Rejected) return;

    const auto editResult = dialog.editResult();
    auto validation = ActiveConfigValidator::validateRepeatBindingCandidate(
        editResult.key,
        editResult.enabled,
        profile,
        context_.activeMacro ? context_.activeMacro() : nullptr);
    if (!validation.ok) {
        AppMessageDialog::warning(context_.parent, QString::fromWCharArray(L"\u6309\u952e\u51b2\u7a81"), validation.message);
        return;
    }
    auto result = context_.controller->upsertBinding(
        bindingId,
        editResult.key,
        editResult.name,
        editResult.mode,
        editResult.delayMs,
        editResult.enabled,
        editResult.suppressMixedMode,
        editResult.clickPos);
    if (!result.ok) {
        AppMessageDialog::warning(context_.parent, QString::fromWCharArray(L"\u6309\u952e\u51b2\u7a81"), result.message);
        return;
    }

    context_.page->setBindings(profile->bindings, profile->id);
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
