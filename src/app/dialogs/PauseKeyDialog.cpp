#include "dialogs/PauseKeyDialog.h"
PauseKeyDialog::PauseKeyDialog(QWidget* parent, std::optional<KeyDescriptor> existing) : KeyCaptureDialogBase(parent) {
    setWindowTitle(existing ? tr("编辑暂停键") : tr("添加暂停键"));
    if (existing) setCapturedKey(*existing);
    prepareEditor({{"needsDelay", false}, {"positionVisible", false}}, nullptr, QSize(320, 178));
}
PauseKeyEditResult PauseKeyDialog::editResult() const { return {*capturedKey()}; }
