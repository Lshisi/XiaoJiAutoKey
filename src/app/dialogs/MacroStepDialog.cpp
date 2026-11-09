#include "dialogs/MacroStepDialog.h"
#include "dialogs/AppMessageDialog.h"
#include "MacroActions.h"
MacroStepDialog::MacroStepDialog(std::optional<MacroStep> existing,
    std::function<bool()> supportsMouseMove, QWidget* parent, std::optional<MacroStep> initial) : KeyCaptureDialogBase(parent) {
    setPrimaryMouseButtonsAllowed(true);
    setWindowTitle(existing ? tr("编辑步骤") : tr("添加步骤"));
    const MacroStep step = existing ? *existing : initial.value_or(MacroStep{});
    if (step.key.IsValid()) setCapturedKey(step.key);
    QVariantMap fields{{"macroStep", true}, {"nameVisible", true}, {"actionGuideVisible", false},
        {"action", static_cast<int>(step.action)}, {"duration", QString::number(step.durationMs)},
        {"wheelDirection", step.wheelDelta < 0 ? 1 : 0}, {"wheelCount", QString::number(std::abs(step.wheelDelta / 120))},
        {"name", QString::fromStdWString(step.name)}, {"delay", QString::number(step.delayAfterMs)},
        {"fixed", step.clickPos.IsFixed()}, {"anchorX", step.clickPos.anchorX}, {"anchorY", step.clickPos.anchorY}};
    prepareEditor(fields, std::move(supportsMouseMove), QSize(420, 340));
}
MacroStep MacroStepDialog::editResult() const {
    MacroStep step;
    step.action = static_cast<MacroAction>(value("action").toInt());
    step.key = capturedKey().value_or(KeyDescriptor{});
    step.name = value("name").toString().trimmed().toStdWString();
    step.durationMs = value("duration").toString().toInt();
    step.delayAfterMs = value("delay").toString().toInt();
    step.wheelDelta = value("wheelCount").toString().toInt() * (value("wheelDirection").toInt() == 1 ? -120 : 120);
    step.clickPos = clickPosition();
    NormalizeMacroStep(step);
    return step;
}
void MacroStepDialog::editField(const QString& field, const QVariant& argument) {
    if (field == "action") {
        const int action = argument.toInt();
        if (action < static_cast<int>(MacroAction::Click) || action > static_cast<int>(MacroAction::Wheel)) return;
        setValue(field, action);
        updateActionGuide();
    } else if (field == "wheelDirection") setValue(field, argument.toInt());
    else if (field == "duration" || field == "wheelCount") setValue(field, argument.toString());
    else KeyCaptureDialogBase::editField(field, argument);
}
void MacroStepDialog::handleAction(const QString& action, const QVariant& argument) {
    if (action == "dismissActionGuide") { dismissActionGuide(); return; }
    if (action != "accept") { KeyCaptureDialogBase::handleAction(action, argument); return; }
    const auto kind = static_cast<MacroAction>(value("action").toInt());
    bool ok = true;
    if (MacroActionNeedsKey(kind) && !capturedKey()) ok = false;
    bool parsed = false;
    if (kind == MacroAction::Hold || kind == MacroAction::Wait) {
        const int duration = value("duration").toString().toInt(&parsed);
        ok &= parsed && duration > 0;
    }
    if (kind != MacroAction::Wait) {
        const int delay = value("delay").toString().toInt(&parsed);
        ok &= parsed && delay >= 0;
    }
    if (kind == MacroAction::Wheel) {
        const int count = value("wheelCount").toString().toInt(&parsed);
        ok &= parsed && count >= 1 && count <= 100;
    }
    if (!ok) { AppMessageDialog::warning(this, tr("参数无效"), tr("请检查按键和时长；时长至少 1ms，动作后间隔可为 0，滚轮格数为 1 到 100。")); return; }
    dismissActionGuide();
    accept();
}

void MacroStepDialog::setActionGuideCallbacks(std::function<bool(MacroAction)> shouldShow,
    std::function<void(MacroAction)> dismissed) {
    shouldShowActionGuide_ = std::move(shouldShow);
    actionGuideDismissed_ = std::move(dismissed);
    updateActionGuide();
}

void MacroStepDialog::updateActionGuide() {
    const auto action = static_cast<MacroAction>(value("action").toInt());
    setValue("actionGuideVisible", shouldShowActionGuide_ && shouldShowActionGuide_(action));
}

void MacroStepDialog::dismissActionGuide() {
    if (!value("actionGuideVisible").toBool()) return;
    if (actionGuideDismissed_) actionGuideDismissed_(static_cast<MacroAction>(value("action").toInt()));
    setValue("actionGuideVisible", false);
}
