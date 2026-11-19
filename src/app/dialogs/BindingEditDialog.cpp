#include "dialogs/BindingEditDialog.h"
BindingEditDialog::BindingEditDialog(std::optional<BindingEditResult> existing, int defaultDelayMs, bool mixedMode,
    std::function<bool()> supportsMouseMove, QWidget* parent) : KeyCaptureDialogBase(parent) {
    setPrimaryMouseButtonsAllowed(true);
    setWindowTitle(existing ? tr("编辑按键") : tr("添加按键"));
    QVariantMap fields{{"modeVisible", true}, {"nameVisible", true}, {"delay", QString::number(defaultDelayMs)},
        {"mixedMode", mixedMode}, {"martialAssist", existing && existing->martialAssist},
        {"preventAccidentalStop", existing && existing->preventAccidentalStop}};
    if (existing) {
        setCapturedKey(existing->key);
        const int mode = existing->mode == RepeatMode::Hold ? 1
            : existing->mode == RepeatMode::Independent ? 2
            : existing->mode == RepeatMode::Sequence ? 3
            : existing->mode == RepeatMode::LongPress ? 4 : 0;
        fields.insert("mode", mode);
        fields.insert("name", existing->name);
        fields.insert("needsDelay", existing->mode != RepeatMode::LongPress);
        fields.insert("delay", QString::number(existing->delayMs));
        fields.insert("enabled", existing->enabled);
        fields.insert("priority", existing->mode != RepeatMode::Hold || !existing->suppressMixedMode);
        fields.insert("fixed", existing->clickPos.IsFixed());
        fields.insert("anchorX", existing->clickPos.anchorX);
        fields.insert("anchorY", existing->clickPos.anchorY);
    }
    prepareEditor(fields, std::move(supportsMouseMove), QSize(420, 300));
}
BindingEditResult BindingEditDialog::editResult() const {
    const int selected = value("mode").toInt();
    const auto mode = selected == 1 ? RepeatMode::Hold
        : selected == 2 ? RepeatMode::Independent
        : selected == 3 ? RepeatMode::Sequence
        : selected == 4 ? RepeatMode::LongPress : RepeatMode::Toggle;
    const int delayMs = mode == RepeatMode::LongPress ? kDefaultDelayMs : value("delay").toString().toInt();
    return {*capturedKey(), value("name").toString().trimmed(), mode, delayMs, value("enabled").toBool(),
        mode == RepeatMode::Hold && !value("priority").toBool(), clickPosition(),
        mode == RepeatMode::LongPress && value("martialAssist").toBool(), value("preventAccidentalStop").toBool()};
}
