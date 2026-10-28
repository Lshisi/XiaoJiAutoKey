#include "dialogs/PauseKeyDialog.h"
PauseKeyDialog::PauseKeyDialog(QWidget* parent) : KeyCaptureDialogBase(parent) {
    setWindowTitle(tr("添加暂停按键"));
    prepareEditor({{"needsDelay", false}});
}
PauseKeyEditResult PauseKeyDialog::editResult() const { return {*capturedKey()}; }
