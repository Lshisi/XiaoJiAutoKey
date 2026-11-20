#include "MacroActions.h"
#include "coordinators/MacroCoordinator.h"

#include "config/ActiveConfigValidator.h"
#include "controllers/MacroController.h"
#include "dialogs/AppMessageDialog.h"
#include "dialogs/BatchEditDialog.h"
#include "dialogs/ConfigConfirmDialog.h"
#include "dialogs/ConfigNameDialog.h"
#include "dialogs/MacroStepDialog.h"
#include "dialogs/MacroRecordingDialog.h"
#include "dialogs/PauseKeyDialog.h"
#include "input/GlobalInputHook.h"
#include "MacroEngine.h"
#include "ui/models/MacroPageModel.h"
#include "ui/models/KeyOverviewData.h"

#include <QDialog>
#include <QScopedValueRollback>

#include <algorithm>

namespace {
std::vector<std::uint64_t> StepIds(const QVariantList& values) {
    std::vector<std::uint64_t> ids;
    ids.reserve(values.size());
    for (const auto& value : values) {
        bool ok = false;
        const std::uint64_t id = value.toString().toULongLong(&ok);
        if (ok) ids.push_back(id);
    }
    return ids;
}
}

MacroCoordinator::MacroCoordinator(QObject* parent) : QObject(parent) {}

void MacroCoordinator::bind(Context context) {
    context_ = std::move(context);
    if (!context_.page) return;

    connect(context_.page, &MacroPageModel::macroSwitched, this, &MacroCoordinator::onMacroSwitched);
    connect(context_.page, &MacroPageModel::macroAddRequested, this, &MacroCoordinator::onMacroAdd);
    connect(context_.page, &MacroPageModel::macroDuplicateRequested, this, &MacroCoordinator::onMacroDuplicate);
    connect(context_.page, &MacroPageModel::macroRenameRequested, this, &MacroCoordinator::onMacroRename);
    connect(context_.page, &MacroPageModel::macroDeleteRequested, this, &MacroCoordinator::onMacroDelete);
    connect(context_.page, &MacroPageModel::macroEnabledToggled, this, &MacroCoordinator::onMacroEnabledToggled);
    connect(context_.page, &MacroPageModel::recordMacroRequested, this, &MacroCoordinator::onRecordMacro);
    connect(context_.page, &MacroPageModel::addStepRequested, this, &MacroCoordinator::onMacroAddStep);
    connect(context_.page, &MacroPageModel::insertStepRequested, this, &MacroCoordinator::onMacroInsertStep);
    connect(context_.page, &MacroPageModel::editStepRequested, this, &MacroCoordinator::onMacroEditStep);
    connect(context_.page, &MacroPageModel::deleteStepRequested, this, &MacroCoordinator::onMacroDeleteStep);
    connect(context_.page, &MacroPageModel::stepEnabledToggled, this, &MacroCoordinator::onStepEnabledToggled);
    connect(context_.page, &MacroPageModel::duplicateStepRequestedImpl, this, &MacroCoordinator::onMacroDuplicateStep);
    connect(context_.page, &MacroPageModel::batchDuplicateRequestedImpl, this, &MacroCoordinator::onMacroBatchDuplicate);
    connect(context_.page, &MacroPageModel::batchDeleteRequestedImpl, this, &MacroCoordinator::onMacroBatchDelete);
    connect(context_.page, &MacroPageModel::batchEditRequestedImpl, this, &MacroCoordinator::onMacroBatchEdit);
    connect(context_.page, &MacroPageModel::stepMoveRequested, this, &MacroCoordinator::onMacroMoveStep);
    connect(context_.page, &MacroPageModel::startStopClicked, this, &MacroCoordinator::onMacroStartStopClicked);
    connect(context_.page, &MacroPageModel::hotkeyClearClicked, this, &MacroCoordinator::onMacroHotkeyClearClicked);
    connect(context_.page, &MacroPageModel::modeChanged, this, &MacroCoordinator::onMacroModeChanged);
    connect(context_.page, &MacroPageModel::loopCountChanged, this, &MacroCoordinator::onMacroLoopCountChanged);
    connect(context_.page, &MacroPageModel::beepToggled, this, &MacroCoordinator::onMacroBeepToggled);
    connect(context_.page, &MacroPageModel::statusOverlayToggled, this, &MacroCoordinator::onStatusOverlayToggled);
    connect(context_.page, &MacroPageModel::pauseKeyAddClicked, this, &MacroCoordinator::onPauseKeyAdd);
    connect(context_.page, &MacroPageModel::pauseKeyEditClicked, this, [this](int index) { editPauseKey(index); });
    connect(context_.page, &MacroPageModel::pauseKeyDeleteClicked, this, &MacroCoordinator::onPauseKeyDelete);
}

void MacroCoordinator::pollRunningState() {
    if (!context_.controller || !context_.engine || !context_.page) return;
    auto* macro = context_.controller->activeMacro();
    if (!macro) return;

    const auto status = context_.engine->GetStatus(macro->id);
    context_.page->setExecutionError(QString::fromStdWString(status.error));
    const bool running = status.running;
    context_.page->setRunning(running);
    if (running != lastMacroRunning_) {
        lastMacroRunning_ = running;
        if (context_.refreshStatusIndicators) context_.refreshStatusIndicators();
        if (context_.playToggleSound) context_.playToggleSound(running);
    } else if (running && status.pauseKeyActive != lastMacroPauseKeyActive_) {
        if (context_.playSound) context_.playSound(status.pauseKeyActive ? SoundEvent::MacroPause : SoundEvent::MacroResume);
    }
    lastMacroPauseKeyActive_ = status.pauseKeyActive;
}

void MacroCoordinator::onMacroSwitched(std::uint64_t macroId) {
    if (!context_.controller) return;
    auto* current = context_.controller->activeMacro();
    if (current && current->id == macroId) return;

    const auto* targetMacro = context_.controller->macroById(macroId);
    auto validation = ActiveConfigValidator::validateActiveHotkeys(
        context_.activeProfile ? context_.activeProfile() : nullptr,
        targetMacro);
    if (!validation.ok) {
        if (context_.refreshMacroPage) context_.refreshMacroPage();
        AppMessageDialog::warning(context_.parent, QString::fromWCharArray(L"\u6309\u952e\u51b2\u7a81"), validation.message);
        return;
    }

    stopAllForConfigChange();

    auto result = context_.controller->switchMacro(macroId);
    if (!result.ok) {
        AppMessageDialog::warning(context_.parent, QString::fromWCharArray(L"\u5207\u6362\u5faa\u73af\u5931\u8d25"), result.message);
        return;
    }
    if (context_.applyMacros) context_.applyMacros();
    if (context_.refreshMacroPage) context_.refreshMacroPage();
}

void MacroCoordinator::onMacroAdd() {
    if (!context_.inputHook || !context_.controller) return;

    context_.inputHook->setDialogOpen(true);
    QString name;
    const bool ok = ConfigNameDialog::getName(context_.parent,
        QString::fromWCharArray(L"\u65b0\u5efa\u5b8f\u73af"),
        QString::fromWCharArray(L"\u8bf7\u8f93\u5165\u5b8f\u73af\u540d\u79f0:"),
        QString(),
        name);
    context_.inputHook->setDialogOpen(false);
    if (!ok || name.trimmed().isEmpty()) return;

    std::uint64_t createdMacroId = 0;
    auto result = context_.controller->addMacro(name, createdMacroId);
    if (!result.ok) {
        AppMessageDialog::warning(context_.parent, QString::fromWCharArray(L"\u65b0\u5efa\u5faa\u73af\u5931\u8d25"), result.message);
        return;
    }
    if (context_.applyMacros) context_.applyMacros();
    if (context_.saveConfig) context_.saveConfig();
    if (context_.refreshMacroPage) context_.refreshMacroPage();
}

void MacroCoordinator::onMacroDuplicate() {
    if (!context_.inputHook || !context_.controller) return;
    auto* macro = context_.controller->activeMacro();
    if (!macro) return;

    context_.inputHook->setDialogOpen(true);
    QString name;
    const bool ok = ConfigNameDialog::getName(context_.parent,
        QString::fromWCharArray(L"\u590d\u5236\u5faa\u73af"),
        QString::fromWCharArray(L"\u65b0\u5faa\u73af\u540d\u79f0:"),
        QString::fromStdWString(macro->name) + QString::fromWCharArray(L"\uff08\u526f\u672c\uff09"),
        name);
    context_.inputHook->setDialogOpen(false);
    if (!ok || name.trimmed().isEmpty()) return;

    stopAllForConfigChange();

    std::uint64_t createdMacroId = 0;
    auto result = context_.controller->duplicateActiveMacro(name, createdMacroId);
    if (!result.ok) {
        AppMessageDialog::warning(context_.parent, QString::fromWCharArray(L"\u590d\u5236\u5faa\u73af\u5931\u8d25"), result.message);
        return;
    }
    if (context_.applyMacros) context_.applyMacros();
    if (context_.saveConfig) context_.saveConfig();
    if (context_.refreshMacroPage) context_.refreshMacroPage();
}

void MacroCoordinator::onMacroRename() {
    if (!context_.inputHook || !context_.controller) return;
    auto* macro = context_.controller->activeMacro();
    if (!macro) return;

    context_.inputHook->setDialogOpen(true);
    QString name;
    const bool ok = ConfigNameDialog::getName(context_.parent,
        QString::fromWCharArray(L"\u91cd\u547d\u540d\u5faa\u73af"),
        QString::fromWCharArray(L"\u65b0\u540d\u79f0:"),
        QString::fromStdWString(macro->name),
        name);
    context_.inputHook->setDialogOpen(false);
    if (!ok || name.trimmed().isEmpty()) return;

    auto result = context_.controller->renameActiveMacro(name);
    if (!result.ok) {
        AppMessageDialog::warning(context_.parent, QString::fromWCharArray(L"\u91cd\u547d\u540d\u5faa\u73af\u5931\u8d25"), result.message);
        return;
    }
    if (context_.saveConfig) context_.saveConfig();
    if (context_.refreshMacroPage) context_.refreshMacroPage();
}

void MacroCoordinator::onMacroDelete() {
    if (!context_.inputHook || !context_.controller) return;
    auto* macro = context_.controller->activeMacro();
    if (!macro) return;

    context_.inputHook->setDialogOpen(true);
    const bool confirmed = ConfigConfirmDialog::confirm(context_.parent,
        QString::fromWCharArray(L"\u5220\u9664\u5faa\u73af"),
        QString::fromWCharArray(L"\u786e\u5b9a\u8981\u5220\u9664\u5faa\u73af\u201c") +
            QString::fromStdWString(macro->name) +
            QString::fromWCharArray(L"\u201d\u5417\uff1f"),
        QString::fromWCharArray(L"\u5220\u9664"));
    context_.inputHook->setDialogOpen(false);
    if (!confirmed) return;

    std::uint64_t newActiveMacroId = 0;
    auto result = context_.controller->deleteActiveMacro(newActiveMacroId);
    if (!result.ok) {
        AppMessageDialog::warning(context_.parent, QString::fromWCharArray(L"\u5220\u9664\u5faa\u73af\u5931\u8d25"), result.message);
        return;
    }
    if (context_.applyMacros) context_.applyMacros();
    if (context_.saveConfig) context_.saveConfig();
    if (context_.refreshMacroPage) context_.refreshMacroPage();
}

void MacroCoordinator::onMacroEnabledToggled(bool enabled) {
    if (!context_.controller || !context_.engine) return;
    auto* macro = context_.controller->activeMacro();
    if (!macro || macro->enabled == enabled) return;

    auto result = context_.controller->setActiveMacroEnabled(enabled);
    if (!result.ok) {
        context_.page->setMacroEnabled(macro->enabled);
        if (context_.refreshMacroProfileCombo) context_.refreshMacroProfileCombo();
        return;
    }

    if (enabled && context_.validateActiveConfig) {
        auto validation = context_.validateActiveConfig();
        if (!validation.ok) {
            context_.controller->setActiveMacroEnabled(false);
            context_.page->setMacroEnabled(false);
            if (context_.refreshMacroProfileCombo) context_.refreshMacroProfileCombo();
            AppMessageDialog::warning(context_.parent,
                QString::fromWCharArray(L"\u65e0\u6cd5\u542f\u7528\u914d\u7f6e"), validation.message);
            return;
        }
    } else if (!enabled) {
        context_.engine->StopAll();
        lastMacroRunning_ = false;
        context_.page->setRunning(false);
    }

    if (context_.applyMacros) context_.applyMacros();
    if (context_.saveConfig) context_.saveConfig();
    context_.page->setMacroEnabled(enabled);
    if (context_.refreshMacroProfileCombo) context_.refreshMacroProfileCombo();
    else if (context_.refreshStatusIndicators) context_.refreshStatusIndicators();
}

void MacroCoordinator::onMacroAddStep() {
    addStep(-1, false);
}

void MacroCoordinator::editFromOverview(const QVariantMap& target, QWidget* parent) {
    QScopedValueRollback<QWidget*> parentScope(context_.parent, parent);
    const QString action = target.value("action").toString();
    if (action == QStringLiteral("add")) {
        MacroStep initial;
        if (target.value("wheel").toInt() != 0) {
            initial.action = MacroAction::Wheel;
            initial.wheelDelta = target.value("wheel").toInt();
        } else initial.key = KeyOverviewData::descriptor(target);
        addStep(-1, true, initial);
    } else if (action == QStringLiteral("output")) {
        context_.page->focusSteps({target.value("targetId").toString().toULongLong()});
        onMacroEditStep(target.value("targetId").toString());
    } else if (action == QStringLiteral("pause")) editPauseKey(target.value("targetId").toInt());
    else if (action == QStringLiteral("hotkey")) emit context_.page->hotkeyEditClicked();
}

void MacroCoordinator::addStep(int insertIndex, bool focusCreated, std::optional<MacroStep> initial) {
    if (!context_.controller || !context_.runKeyCaptureDialog) return;
    if (context_.engine && context_.engine->HasActiveMacro()) return;

    MacroStepDialog dialog(std::nullopt, context_.driverSupportsMouseMove, context_.parent, initial);
    context_.runKeyCaptureDialog(dialog);
    if (dialog.result() == QDialog::Rejected) return;

    const auto editResult = dialog.editResult();
    auto validation = !MacroActionNeedsKey(editResult.action) ? ControllerResult::success() : ActiveConfigValidator::validateChildKeyCandidate(
        editResult.key,
        context_.activeProfile ? context_.activeProfile() : nullptr,
        context_.controller->activeMacro());
    if (!validation.ok) {
        AppMessageDialog::warning(context_.parent, QString::fromWCharArray(L"\u6309\u952e\u51b2\u7a81"), validation.message);
        return;
    }
    std::uint64_t createdStepId = 0;
    auto result = context_.controller->addStep(editResult, createdStepId, insertIndex);
    if (!result.ok) {
        AppMessageDialog::warning(context_.parent, QString::fromWCharArray(L"\u6309\u952e\u51b2\u7a81"), result.message);
        return;
    }
    if (context_.applyMacros) context_.applyMacros();
    if (context_.saveConfig) context_.saveConfig();
    if (context_.refreshMacroPage) context_.refreshMacroPage();
    if (focusCreated) context_.page->focusSteps({createdStepId});
}

void MacroCoordinator::onMacroInsertStep(const QString& stepId, bool before) {
    if (!context_.controller || !context_.runKeyCaptureDialog) return;
    auto* macro = context_.controller->activeMacro();
    if (!macro) return;
    bool ok = false;
    const std::uint64_t id = stepId.toULongLong(&ok);
    if (!ok) return;
    const auto stepIt = std::find_if(macro->steps.begin(), macro->steps.end(),
        [id](const MacroStep& step) { return step.id == id; });
    if (stepIt == macro->steps.end()) return;
    addStep(before ? static_cast<int>(stepIt - macro->steps.begin())
        : static_cast<int>(stepIt - macro->steps.begin()) + 1, true);
}

void MacroCoordinator::onMacroEditStep(const QString& stepId) {
    if (!context_.controller || !context_.runKeyCaptureDialog) return;
    if (context_.engine && context_.engine->HasActiveMacro()) return;
    auto* macro = context_.controller->activeMacro();
    if (!macro) return;
    bool ok = false;
    const std::uint64_t id = stepId.toULongLong(&ok);
    if (!ok) return;
    const auto stepIt = std::find_if(macro->steps.begin(), macro->steps.end(),
        [id](const MacroStep& step) { return step.id == id; });
    if (stepIt == macro->steps.end()) return;
    const auto& step = *stepIt;

    MacroStepDialog dialog(step,
        context_.driverSupportsMouseMove, context_.parent);
    context_.runKeyCaptureDialog(dialog);
    if (dialog.result() == QDialog::Rejected) return;

    const auto editResult = dialog.editResult();
    auto validation = !MacroActionNeedsKey(editResult.action) ? ControllerResult::success() : ActiveConfigValidator::validateChildKeyCandidate(
        editResult.key,
        context_.activeProfile ? context_.activeProfile() : nullptr,
        macro);
    if (!validation.ok) {
        AppMessageDialog::warning(context_.parent, QString::fromWCharArray(L"\u6309\u952e\u51b2\u7a81"), validation.message);
        return;
    }
    auto result = context_.controller->updateStep(id, editResult);
    if (!result.ok) {
        AppMessageDialog::warning(context_.parent, QString::fromWCharArray(L"\u6309\u952e\u51b2\u7a81"), result.message);
        return;
    }
    if (context_.applyMacros) context_.applyMacros();
    if (context_.saveConfig) context_.saveConfig();
    if (context_.refreshMacroPage) context_.refreshMacroPage();
}

void MacroCoordinator::onMacroDeleteStep(const QString& stepId) {
    if (!context_.controller) return;
    bool ok = false;
    const auto id = stepId.toULongLong(&ok);
    if (!ok) return;
    auto result = context_.controller->deleteStep(id);
    if (!result.ok) return;
    if (context_.applyMacros) context_.applyMacros();
    if (context_.saveConfig) context_.saveConfig();
    if (context_.refreshMacroPage) context_.refreshMacroPage();
}

void MacroCoordinator::onMacroDuplicateStep(const QString& stepId) {
    if (!context_.controller) return;
    bool ok = false;
    const auto id = stepId.toULongLong(&ok);
    if (!ok) return;
    std::vector<std::uint64_t> createdIds;
    auto result = context_.controller->duplicateStep(id, createdIds);
    if (!result.ok) return;
    if (context_.applyMacros) context_.applyMacros();
    if (context_.saveConfig) context_.saveConfig();
    if (context_.refreshMacroPage) context_.refreshMacroPage();
    context_.page->focusSteps(createdIds);
}

void MacroCoordinator::onMacroBatchDuplicate(const QVariantList& stepIds) {
    if (!context_.controller || stepIds.isEmpty()) return;
    std::vector<std::uint64_t> createdIds;
    auto result = context_.controller->duplicateSteps(StepIds(stepIds), createdIds);
    if (!result.ok) return;
    if (context_.applyMacros) context_.applyMacros();
    if (context_.saveConfig) context_.saveConfig();
    if (context_.refreshMacroPage) context_.refreshMacroPage();
    context_.page->selectSteps(createdIds);
}

void MacroCoordinator::onMacroBatchDelete(const QVariantList& stepIds) {
    if (!context_.controller || !context_.inputHook || stepIds.isEmpty()) return;
    context_.inputHook->setDialogOpen(true);
    const bool confirmed = ConfigConfirmDialog::confirm(context_.parent,
        QString::fromWCharArray(L"\u6279\u91cf\u5220\u9664\u6b65\u9aa4"),
        QString::fromWCharArray(L"\u786e\u5b9a\u8981\u5220\u9664\u9009\u4e2d\u7684 %1 \u4e2a\u6b65\u9aa4\u5417\uff1f").arg(stepIds.size()),
        QString::fromWCharArray(L"\u5220\u9664"));
    context_.inputHook->setDialogOpen(false);
    if (!confirmed) return;

    auto result = context_.controller->deleteSteps(StepIds(stepIds));
    if (!result.ok) return;
    if (context_.applyMacros) context_.applyMacros();
    if (context_.saveConfig) context_.saveConfig();
    if (context_.refreshMacroPage) context_.refreshMacroPage();
}

void MacroCoordinator::onMacroBatchEdit(const QVariantList& stepIds) {
    if (!context_.controller || !context_.inputHook || stepIds.isEmpty()) return;

    context_.inputHook->setDialogOpen(true);
    BatchEditDialog dialog(true, static_cast<int>(stepIds.size()), context_.parent);
    const int result = dialog.exec();
    context_.inputHook->setDialogOpen(false);
    if (result != QDialog::Accepted) return;

    auto editResult = context_.controller->updateSteps(StepIds(stepIds), dialog.delayMs(), dialog.enabled(), dialog.durationMs());
    if (!editResult.ok) {
        AppMessageDialog::warning(context_.parent, QString::fromWCharArray(L"\u6279\u91cf\u4fee\u6539\u5931\u8d25"), editResult.message);
        if (context_.refreshMacroPage) context_.refreshMacroPage();
        return;
    }
    if (context_.applyMacros) context_.applyMacros();
    if (context_.saveConfig) context_.saveConfig();
    if (context_.refreshMacroPage) context_.refreshMacroPage();
}

void MacroCoordinator::onMacroMoveStep(const QString& stepId, int targetIndex) {
    if (!context_.controller) return;
    bool ok = false;
    const auto id = stepId.toULongLong(&ok);
    if (!ok) return;
    auto result = context_.controller->moveStep(id, targetIndex);
    if (!result.ok) return;
    if (context_.applyMacros) context_.applyMacros();
    if (context_.saveConfig) context_.saveConfig();
    if (context_.refreshMacroPage) context_.refreshMacroPage();
}

void MacroCoordinator::onStepEnabledToggled(const QString& stepId, bool enabled) {
    if (!context_.controller || !context_.engine) return;
    const auto* macro = context_.controller->activeMacro();
    if (!macro || context_.engine->IsRunning(macro->id)) return;
    const auto result = context_.controller->setStepEnabled(stepId.toULongLong(), enabled);
    if (!result.ok) {
        AppMessageDialog::warning(context_.parent, tr("无法修改步骤"), result.message);
    } else {
        if (context_.applyMacros) context_.applyMacros();
        if (context_.saveConfig) context_.saveConfig();
    }
    if (context_.refreshMacroPage) context_.refreshMacroPage();
}

void MacroCoordinator::onMacroStartStopClicked() {
    if (!context_.controller || !context_.engine || !context_.page) return;
    auto* macro = context_.controller->activeMacro();
    if (!macro) return;
    if (!macro->enabled) return;

    const auto status = context_.engine->GetStatus(macro->id);
    context_.page->setExecutionError(QString::fromStdWString(status.error));
    const bool running = status.running;
    if (running) {
        context_.engine->StopAll();
        lastMacroRunning_ = false;
        lastMacroPauseKeyActive_ = false;
        if (context_.playToggleSound) context_.playToggleSound(false);
    } else {
        if (std::none_of(macro->steps.begin(), macro->steps.end(),
            [](const MacroStep& step) { return step.enabled; })) {
            AppMessageDialog::warning(context_.parent, tr("无法启动"), tr("请先启用至少一个循环步骤。"));
            return;
        }
        if (context_.canUseDdDriver && !context_.canUseDdDriver()) {
            if (context_.showDdUnavailableMessage) context_.showDdUnavailableMessage();
            return;
        }
        if (!macro->triggerKey.has_value()) {
            AppMessageDialog::warning(context_.parent,
                QString::fromWCharArray(L"\u65e0\u6cd5\u542f\u52a8"),
                QString::fromWCharArray(L"\u8bf7\u5148\u8bbe\u7f6e\u5faa\u73af\u8fde\u53d1\u7684\u542f\u505c\u5feb\u6377\u952e\u3002"));
            return;
        }
        if (context_.validateActiveConfig) {
            auto validation = context_.validateActiveConfig();
            if (!validation.ok) {
                AppMessageDialog::warning(context_.parent, QString::fromWCharArray(L"\u6309\u952e\u51b2\u7a81"), validation.message);
                return;
            }
        }
        context_.engine->OnTriggerDown(*macro->triggerKey);
        if (macro->triggerMode == MacroTriggerMode::Toggle) context_.engine->OnTriggerUp(*macro->triggerKey);
        const auto started = context_.engine->GetStatus(macro->id);
        if (!started.running) {
            context_.page->setExecutionError(QString::fromStdWString(started.error));
            context_.page->setRunning(false);
            if (!started.error.empty())
                AppMessageDialog::warning(context_.parent, tr("无法启动"), QString::fromStdWString(started.error));
            return;
        }
        lastMacroRunning_ = true;
        lastMacroPauseKeyActive_ = started.pauseKeyActive;
        if (context_.playToggleSound) context_.playToggleSound(true);
    }
    context_.page->setRunning(!running);
    if (context_.refreshStatusIndicators) context_.refreshStatusIndicators();
}

void MacroCoordinator::onMacroHotkeyClearClicked() {
    if (!context_.controller) return;
    auto result = context_.controller->clearTriggerKey();
    if (!result.ok) return;
    if (context_.applyMacros) context_.applyMacros();
    if (context_.saveConfig) context_.saveConfig();
    if (context_.refreshMacroPage) context_.refreshMacroPage();
}

void MacroCoordinator::onMacroModeChanged(int modeIndex) {
    if (!context_.controller) return;
    auto result = context_.controller->setTriggerMode(modeIndex == 1 ? MacroTriggerMode::Hold : MacroTriggerMode::Toggle);
    if (!result.ok) return;
    context_.page->setMode(modeIndex == 1 ? MacroTriggerMode::Hold : MacroTriggerMode::Toggle);
    if (context_.applyMacros) context_.applyMacros();
    if (context_.saveConfig) context_.saveConfig();
}

void MacroCoordinator::onMacroLoopCountChanged(int count) {
    if (!context_.controller) return;
    auto result = context_.controller->setLoopCount(count);
    if (!result.ok) return;
    context_.page->setLoopCount(count);
    if (context_.applyMacros) context_.applyMacros();
    if (context_.saveConfig) context_.saveConfig();
}

void MacroCoordinator::onMacroBeepToggled(bool enabled) {
    if (!context_.page) return;
    if (context_.setMacroBeepEnabled) context_.setMacroBeepEnabled(enabled);
    context_.page->setBeepEnabled(enabled);
}

void MacroCoordinator::onStatusOverlayToggled(bool enabled) {
    if (!context_.page) return;
    if (context_.setStatusOverlayEnabled) context_.setStatusOverlayEnabled(enabled);
    context_.page->setStatusOverlayEnabled(enabled);
    if (context_.saveConfig) context_.saveConfig();
    if (context_.refreshMacroPage) context_.refreshMacroPage();
}

void MacroCoordinator::onPauseKeyAdd() {
    editPauseKey(std::nullopt);
}

void MacroCoordinator::editPauseKey(std::optional<int> index) {
    if (!context_.controller || !context_.runKeyCaptureDialog) return;
    if (context_.engine && context_.engine->HasActiveMacro()) return;
    auto* macro = context_.controller->activeMacro();
    if (!macro) return;
    if (index && (*index < 0 || *index >= static_cast<int>(macro->pauseKeys.size()))) return;

    PauseKeyDialog dialog(context_.parent, index ? std::optional(macro->pauseKeys[*index]) : std::nullopt);
    context_.runKeyCaptureDialog(dialog);
    if (dialog.result() == QDialog::Rejected) return;

    auto validation = ActiveConfigValidator::validateChildKeyCandidate(
        dialog.editResult().key,
        context_.activeProfile ? context_.activeProfile() : nullptr,
        macro);
    if (!validation.ok) {
        AppMessageDialog::warning(context_.parent, QString::fromWCharArray(L"\u6309\u952e\u51b2\u7a81"), validation.message);
        return;
    }

    auto result = context_.controller->upsertPauseKey(index, dialog.editResult().key);
    if (!result.ok) {
        AppMessageDialog::warning(context_.parent, QString::fromWCharArray(L"\u6309\u952e\u51b2\u7a81"), result.message);
        return;
    }
    if (context_.applyMacros) context_.applyMacros();
    if (context_.saveConfig) context_.saveConfig();
    context_.page->setPauseKeys(macro->pauseKeys);
}

void MacroCoordinator::onPauseKeyDelete(int index) {
    if (!context_.controller) return;
    auto result = context_.controller->deletePauseKey(index);
    if (!result.ok) return;
    auto* macro = context_.controller->activeMacro();
    if (!macro) return;
    if (context_.applyMacros) context_.applyMacros();
    if (context_.saveConfig) context_.saveConfig();
    context_.page->setPauseKeys(macro->pauseKeys);
}

void MacroCoordinator::stopAllForConfigChange() {
    if (context_.setRepeatRunning) context_.setRepeatRunning(false);
    if (context_.engine) context_.engine->StopAll();
    if (context_.page) context_.page->setRunning(false);
    lastMacroRunning_ = false;
    if (context_.refreshStatusIndicators) context_.refreshStatusIndicators();
}

void MacroCoordinator::onRecordMacro() {
    if (!context_.inputHook || !context_.controller || !context_.engine) return;
    const auto* macro = context_.controller->activeMacro();
    if (!macro) return;
    if (macro->steps.size() + 4 > kMaxMacroSteps) {
        AppMessageDialog::warning(context_.parent, tr("无法录制"), tr("循环最多包含 512 步，请先删除部分步骤。"));
        return;
    }
    stopAllForConfigChange();
    context_.inputHook->setDialogOpen(true);
    MacroRecordingDialog dialog(*context_.inputHook,
        context_.driverSupportsMouseMove && context_.driverSupportsMouseMove(), kMaxMacroSteps - macro->steps.size(), context_.parent);
    connect(&dialog, &MacroRecordingDialog::recordingStateChanged, this, [this](bool recording) {
        if (context_.playSound) context_.playSound(recording ? SoundEvent::RecordingOn : SoundEvent::RecordingOff);
    });
    const int result = dialog.exec();
    context_.inputHook->setRecordingActive(false);
    context_.inputHook->setDialogOpen(false);
    if (result != QDialog::Accepted) return;
    for (const auto& step : dialog.steps()) {
        if (!MacroActionNeedsKey(step.action)) continue;
        auto validation = ActiveConfigValidator::validateChildKeyCandidate(step.key,
            context_.activeProfile ? context_.activeProfile() : nullptr, context_.controller->activeMacro());
        if (!validation.ok) { AppMessageDialog::warning(context_.parent, tr("无法保存录制"), validation.message); return; }
    }
    const auto validation = context_.controller->appendRecordedSteps(dialog.steps());
    if (!validation.ok) { AppMessageDialog::warning(context_.parent, tr("无法保存录制"), validation.message); return; }
    if (context_.applyMacros) context_.applyMacros();
    if (context_.saveConfig) context_.saveConfig();
    if (context_.refreshMacroPage) context_.refreshMacroPage();
}
