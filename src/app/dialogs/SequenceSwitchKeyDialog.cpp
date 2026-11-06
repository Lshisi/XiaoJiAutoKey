#include "dialogs/SequenceSwitchKeyDialog.h"
SequenceSwitchKeyDialog::SequenceSwitchKeyDialog(QWidget* parent, std::optional<KeyDescriptor> existing) : KeyCaptureDialogBase(parent) {
    setWindowTitle(tr("设置切换键"));
    if (existing) setCapturedKey(*existing);
    prepareEditor({{"needsDelay", false}, {"positionVisible", false}}, nullptr, QSize(320, 178));
}
SequenceSwitchKeyEditResult SequenceSwitchKeyDialog::editResult() const { return {*capturedKey()}; }
