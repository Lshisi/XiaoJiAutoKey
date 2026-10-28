#include "dialogs/MacroStepDialog.h"
MacroStepDialog::MacroStepDialog(std::optional<MacroStepEditResult> existing,
    std::function<bool()> supportsMouseMove, QWidget* parent) : KeyCaptureDialogBase(parent) {
    setPrimaryMouseButtonsAllowed(true);
    setWindowTitle(existing ? tr("编辑步骤") : tr("添加步骤"));
    QVariantMap fields;
    if (existing) {
        setCapturedKey(existing->key);
        fields.insert("delay", QString::number(existing->delayAfterMs));
        fields.insert("fixed", existing->clickPos.IsFixed());
        fields.insert("anchorX", existing->clickPos.anchorX);
        fields.insert("anchorY", existing->clickPos.anchorY);
    }
    prepareEditor(fields, std::move(supportsMouseMove));
}
MacroStepEditResult MacroStepDialog::editResult() const {
    return {*capturedKey(), value("delay").toString().toInt(), clickPosition()};
}
