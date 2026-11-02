import QtQuick
import XiaoJi.UI

Window {
    id: dialog
    required property Window ownerWindow
    transientParent: ownerWindow
    modality: Qt.ApplicationModal
    flags: Qt.Dialog | Qt.FramelessWindowHint
    visible: true
    color: "transparent"
    title: dialogModel.dialogState.title
    width: Math.min(dialogModel.dialogState.width, ownerWindow.width - 32)
    height: Math.min(ownerWindow.height - 32, Math.max(160, shell.preferredHeight))
    x: ownerWindow.x + (ownerWindow.width - width) / 2
    y: ownerWindow.y + (ownerWindow.height - height) / 2
    DialogShell { id: shell; anchors.fill: parent }
    onClosing: event => { event.accepted = false; dialogModel.dispatch("cancel") }
}
