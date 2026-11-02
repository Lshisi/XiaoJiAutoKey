import QtQuick
import XiaoJi.UI

Window {
    id: dialog
    required property Window ownerWindow
    required property Item anchorItem
    transientParent: ownerWindow
    modality: Qt.ApplicationModal
    flags: Qt.Dialog | Qt.FramelessWindowHint
    visible: true
    color: "transparent"
    title: dialogModel.dialogState.title
    width: Math.min(dialogModel.dialogState.width, ownerWindow.width - 32)
    height: Math.min(ownerWindow.height - 32, Math.max(160, shell.preferredHeight))
    readonly property point anchorCenter: anchorItem
        ? anchorItem.mapToItem(ownerWindow.contentItem, anchorItem.width / 2, anchorItem.height / 2)
        : Qt.point(ownerWindow.width / 2, ownerWindow.height / 2)
    x: ownerWindow.x + anchorCenter.x - width / 2
    y: ownerWindow.y + anchorCenter.y - height / 2
    DialogShell { id: shell; anchors.fill: parent }
    onClosing: event => { event.accepted = false; dialogModel.dispatch("cancel") }
}
