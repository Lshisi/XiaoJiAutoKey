#include "dialogs/PauseKeyDialog.h"
PauseKeyDialog::PauseKeyDialog(QWidget* parent) : KeyCaptureDialogBase(parent) {
    setWindowTitle(tr("添加暂停按键"));
    prepareEditor({{"needsDelay", false}, {"positionVisible", false}}, nullptr, QSize(320, 178));
}
PauseKeyEditResult PauseKeyDialog::editResult() const { return {*capturedKey()}; }
