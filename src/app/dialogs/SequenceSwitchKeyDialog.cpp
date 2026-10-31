#include "dialogs/SequenceSwitchKeyDialog.h"
SequenceSwitchKeyDialog::SequenceSwitchKeyDialog(QWidget* parent) : KeyCaptureDialogBase(parent) {
    setWindowTitle(tr("设置切换键"));
    prepareEditor({{"needsDelay", false}, {"positionVisible", false}}, nullptr, QSize(320, 178));
}
SequenceSwitchKeyEditResult SequenceSwitchKeyDialog::editResult() const { return {*capturedKey()}; }
