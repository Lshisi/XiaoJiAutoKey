#include "coordinators/MacroCoordinator.h"

#include "config/ActiveConfigValidator.h"
#include "controllers/MacroController.h"
#include "dialogs/AppMessageDialog.h"
#include "dialogs/MacroStepDialog.h"
#include "input/GlobalInputHook.h"
#include "MacroEngine.h"
#include "ui/pages/MacroPage.h"

#include <QDialog>
#include <QInputDialog>
#include <QLineEdit>
#include <QMessageBox>

MacroCoordinator::MacroCoordinator(QObject* parent) : QObject(parent) {}

void MacroCoordinator::bind(Context context) {
    context_ = std::move(context);
    if (!context_.page) return;

    connect(context_.page, &MacroPage::macroSwitched, this, &MacroCoordinator::onMacroSwitched);
    connect(context_.page, &MacroPage::macroAddRequested, this, &MacroCoordinator::onMacroAdd);
    connect(context_.page, &MacroPage::macroRenameRequested, this, &MacroCoordinator::onMacroRename);
    connect(context_.page, &MacroPage::macroDeleteRequested, this, &MacroCoordinator::onMacroDelete);
    connect(context_.page, &MacroPage::addStepRequested, this, &MacroCoordinator::onMacroAddStep);
    connect(context_.page, &MacroPage::editStepRequested, this, &MacroCoordinator::onMacroEditStep);
    connect(context_.page, &MacroPage::deleteStepRequested, this, &MacroCoordinator::onMacroDeleteStep);
    connect(context_.page, &MacroPage::stepMoveRequested, this, &MacroCoordinator::onMacroMoveStep);
    connect(context_.page, &MacroPage::startStopClicked, this, &MacroCoordinator::onMacroStartStopClicked);
    connect(context_.page, &MacroPage::modeChanged, this, &MacroCoordinator::onMacroModeChanged);
    connect(context_.page, &MacroPage::loopCountChanged, this, &MacroCoordinator::onMacroLoopCountChanged);
    connect(context_.page, &MacroPage::beepToggled, this, &MacroCoordinator::onMacroBeepToggled);
}

void MacroCoordinator::pollRunningState() {
    if (!context_.controller || !context_.engine || !context_.page) return;
    auto* macro = context_.controller->activeMacro();
    if (!macro) return;

    const bool running = context_.engine->IsRunning(macro->id);
    context_.page->setRunning(running);
    if (running != lastMacroRunning_) {
        lastMacroRunning_ = running;
        if (context_.playToggleSound) context_.playToggleSound(running);
    }
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
        AppMessageDialog::warning(context_.parent, QString::fromWCharArray(L"\u5207\u6362\u5b8f\u5931\u8d25"), result.message);
        return;
    }
    if (context_.applyMacros) context_.applyMacros();
    if (context_.refreshMacroPage) context_.refreshMacroPage();
}

void MacroCoordinator::onMacroAdd() {
    if (!context_.inputHook || !context_.controller) return;

    context_.inputHook->setDialogOpen(true);
    bool ok = false;
    QString name = QInputDialog::getText(context_.parent,
        QString::fromWCharArray(L"\u65b0\u5efa\u5b8f"),
        QString::fromWCharArray(L"\u8bf7\u8f93\u5165\u5b8f\u540d\u79f0:"),
        QLineEdit::Normal, QString(), &ok);
    context_.inputHook->setDialogOpen(false);
    if (!ok || name.trimmed().isEmpty()) return;

    std::uint64_t createdMacroId = 0;
    auto result = context_.controller->addMacro(name, createdMacroId);
    if (!result.ok) {
        AppMessageDialog::warning(context_.parent, QString::fromWCharArray(L"\u65b0\u5efa\u5b8f\u5931\u8d25"), result.message);
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
    bool ok = false;
    QString name = QInputDialog::getText(context_.parent,
        QString::fromWCharArray(L"\u91cd\u547d\u540d\u5b8f"),
        QString::fromWCharArray(L"\u65b0\u540d\u79f0:"),
        QLineEdit::Normal, QString::fromStdWString(macro->name), &ok);
    context_.inputHook->setDialogOpen(false);
    if (!ok || name.trimmed().isEmpty()) return;

    auto result = context_.controller->renameActiveMacro(name);
    if (!result.ok) {
        AppMessageDialog::warning(context_.parent, QString::fromWCharArray(L"\u91cd\u547d\u540d\u5b8f\u5931\u8d25"), result.message);
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
    auto answer = QMessageBox::question(context_.parent,
        QString::fromWCharArray(L"\u5220\u9664\u5b8f"),
        QString::fromWCharArray(L"\u786e\u5b9a\u8981\u5220\u9664\u5b8f\u201c") +
            QString::fromStdWString(macro->name) +
            QString::fromWCharArray(L"\u201d\u5417\uff1f"),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    context_.inputHook->setDialogOpen(false);
    if (answer != QMessageBox::Yes) return;

    std::uint64_t newActiveMacroId = 0;
    auto result = context_.controller->deleteActiveMacro(newActiveMacroId);
    if (!result.ok) {
        AppMessageDialog::warning(context_.parent, QString::fromWCharArray(L"\u5220\u9664\u5b8f\u5931\u8d25"), result.message);
        return;
    }
    if (context_.applyMacros) context_.applyMacros();
    if (context_.saveConfig) context_.saveConfig();
    if (context_.refreshMacroPage) context_.refreshMacroPage();
}

void MacroCoordinator::onMacroAddStep() {
    if (!context_.controller || !context_.runKeyCaptureDialog) return;

    MacroStepDialog dialog(std::nullopt, context_.parent);
    context_.runKeyCaptureDialog(dialog);
    if (dialog.result() == QDialog::Rejected) return;

    const auto editResult = dialog.editResult();
    auto validation = ActiveConfigValidator::validateChildKeyCandidate(
        editResult.key,
        context_.activeProfile ? context_.activeProfile() : nullptr,
        context_.controller->activeMacro());
    if (!validation.ok) {
        AppMessageDialog::warning(context_.parent, QString::fromWCharArray(L"\u6309\u952e\u51b2\u7a81"), validation.message);
        return;
    }

    auto result = context_.controller->addStep(editResult.key, editResult.delayAfterMs);
    if (!result.ok) {
        AppMessageDialog::warning(context_.parent, QString::fromWCharArray(L"\u6309\u952e\u51b2\u7a81"), result.message);
        return;
    }
    if (context_.applyMacros) context_.applyMacros();
    if (context_.saveConfig) context_.saveConfig();
    if (context_.refreshMacroPage) context_.refreshMacroPage();
}

void MacroCoordinator::onMacroEditStep(int stepIndex) {
    if (!context_.controller || !context_.runKeyCaptureDialog) return;
    auto* macro = context_.controller->activeMacro();
    if (!macro || stepIndex < 0 || stepIndex >= static_cast<int>(macro->steps.size())) return;
    const auto& step = macro->steps[stepIndex];

    MacroStepDialog dialog(MacroStepEditResult{step.key, step.delayAfterMs}, context_.parent);
    context_.runKeyCaptureDialog(dialog);
    if (dialog.result() == QDialog::Rejected) return;

    const auto editResult = dialog.editResult();
    auto validation = ActiveConfigValidator::validateChildKeyCandidate(
        editResult.key,
        context_.activeProfile ? context_.activeProfile() : nullptr,
        macro);
    if (!validation.ok) {
        AppMessageDialog::warning(context_.parent, QString::fromWCharArray(L"\u6309\u952e\u51b2\u7a81"), validation.message);
        return;
    }

    auto result = context_.controller->updateStep(stepIndex, editResult.key, editResult.delayAfterMs);
    if (!result.ok) {
        AppMessageDialog::warning(context_.parent, QString::fromWCharArray(L"\u6309\u952e\u51b2\u7a81"), result.message);
        return;
    }
    if (context_.applyMacros) context_.applyMacros();
    if (context_.saveConfig) context_.saveConfig();
    if (context_.refreshMacroPage) context_.refreshMacroPage();
}

void MacroCoordinator::onMacroDeleteStep(int stepIndex) {
    if (!context_.controller) return;
    auto result = context_.controller->deleteStep(stepIndex);
    if (!result.ok) return;
    if (context_.applyMacros) context_.applyMacros();
    if (context_.saveConfig) context_.saveConfig();
    if (context_.refreshMacroPage) context_.refreshMacroPage();
}

void MacroCoordinator::onMacroMoveStep(int fromIndex, int toIndex) {
    if (!context_.controller) return;
    auto result = context_.controller->moveStep(fromIndex, toIndex);
    if (!result.ok) return;
    if (context_.applyMacros) context_.applyMacros();
    if (context_.saveConfig) context_.saveConfig();
    if (context_.refreshMacroPage) context_.refreshMacroPage();
}

void MacroCoordinator::onMacroStartStopClicked() {
    if (!context_.controller || !context_.engine || !context_.page) return;
    auto* macro = context_.controller->activeMacro();
    if (!macro) return;

    const bool running = context_.engine->IsRunning(macro->id);
    if (running) {
        context_.engine->StopAll();
    } else {
        if (!macro->triggerKey.has_value()) {
            AppMessageDialog::warning(context_.parent,
                QString::fromWCharArray(L"\u65e0\u6cd5\u542f\u52a8"),
                QString::fromWCharArray(L"\u8bf7\u5148\u8bbe\u7f6e\u5b8f\u5faa\u73af\u7684\u542f\u505c\u5feb\u6377\u952e\u3002"));
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
    }
    context_.page->setRunning(!running);
}

void MacroCoordinator::onMacroModeChanged(int modeIndex) {
    if (!context_.controller) return;
    auto result = context_.controller->setTriggerMode(modeIndex == 1 ? MacroTriggerMode::Hold : MacroTriggerMode::Toggle);
    if (!result.ok) return;
    if (context_.applyMacros) context_.applyMacros();
    if (context_.saveConfig) context_.saveConfig();
}

void MacroCoordinator::onMacroLoopCountChanged(int count) {
    if (!context_.controller) return;
    auto result = context_.controller->setLoopCount(count);
    if (!result.ok) return;
    if (context_.applyMacros) context_.applyMacros();
    if (context_.saveConfig) context_.saveConfig();
}

void MacroCoordinator::onMacroBeepToggled(bool enabled) {
    if (!context_.page) return;
    if (context_.setMacroBeepEnabled) context_.setMacroBeepEnabled(enabled);
    context_.page->setBeepEnabled(enabled);
}

void MacroCoordinator::stopAllForConfigChange() {
    if (context_.setRepeatRunning) context_.setRepeatRunning(false);
    if (context_.engine) context_.engine->StopAll();
    if (context_.page) context_.page->setRunning(false);
    lastMacroRunning_ = false;
}
