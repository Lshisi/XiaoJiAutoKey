#include "dialogs/BindingEditDialog.h"
BindingEditDialog::BindingEditDialog(std::optional<BindingEditResult> existing, int defaultDelayMs,
    std::function<bool()> supportsMouseMove, QWidget* parent) : KeyCaptureDialogBase(parent) {
    setPrimaryMouseButtonsAllowed(true);
    setWindowTitle(existing ? tr("编辑按键") : tr("添加按键"));
    QVariantMap fields{{"modeVisible", true}, {"delay", QString::number(defaultDelayMs)}};
    if (existing) {
        setCapturedKey(existing->key);
        const int mode = existing->mode == RepeatMode::Hold ? 1 : existing->mode == RepeatMode::Independent ? 2 : existing->mode == RepeatMode::Sequence ? 3 : 0;
        fields.insert("mode", mode);
        fields.insert("delay", QString::number(existing->delayMs));
        fields.insert("enabled", existing->enabled);
        fields.insert("priority", existing->mode != RepeatMode::Hold || !existing->suppressMixedMode);
        fields.insert("fixed", existing->clickPos.IsFixed());
        fields.insert("anchorX", existing->clickPos.anchorX);
        fields.insert("anchorY", existing->clickPos.anchorY);
    }
    prepareEditor(fields, std::move(supportsMouseMove));
}
BindingEditResult BindingEditDialog::editResult() const {
    const int selected = value("mode").toInt();
    const auto mode = selected == 1 ? RepeatMode::Hold : selected == 2 ? RepeatMode::Independent : selected == 3 ? RepeatMode::Sequence : RepeatMode::Toggle;
    return {*capturedKey(), mode, value("delay").toString().toInt(), value("enabled").toBool(),
        mode == RepeatMode::Hold && !value("priority").toBool(), clickPosition()};
}
