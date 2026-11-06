import QtQuick
import XiaoJi.UI

Window {
    id: dialog
    required property Window ownerWindow
    required property Item anchorItem
    property var model: dialogModel
    readonly property bool overviewWindow: model.dialogState.page === "DialogKeyOverview.qml"
    property Window sizeReferenceWindow: ownerWindow
    transientParent: ownerWindow
    modality: overviewWindow ? Qt.NonModal : Qt.ApplicationModal
    flags: Qt.Dialog | Qt.FramelessWindowHint
    visible: true
    color: "transparent"
    title: model.dialogState.title
    width: Math.min(model.dialogState.width, overviewWindow ? Screen.desktopAvailableWidth - 32 : sizeReferenceWindow.width - 32)
    height: overviewWindow ? Math.min(Screen.desktopAvailableHeight - 32, Math.max(240, shell.preferredHeight))
        : Math.min(sizeReferenceWindow.height - 32, Math.max(160, shell.preferredHeight))
    minimumWidth: overviewWindow ? 340 : 0
    minimumHeight: overviewWindow ? 240 : 0
    maximumWidth: Screen.desktopAvailableWidth - 32
    maximumHeight: Screen.desktopAvailableHeight - 32
    readonly property point anchorCenter: anchorItem
        ? anchorItem.mapToItem(ownerWindow.contentItem, anchorItem.width / 2, anchorItem.height / 2)
        : Qt.point(ownerWindow.width / 2, ownerWindow.height / 2)
    x: overviewWindow ? Math.max(Screen.virtualX + 16, Math.min(ownerWindow.x + anchorCenter.x - width / 2,
        Screen.virtualX + Screen.desktopAvailableWidth - width - 16)) : ownerWindow.x + anchorCenter.x - width / 2
    y: overviewWindow ? Math.max(Screen.virtualY + 16, Math.min(ownerWindow.y + anchorCenter.y - height / 2,
        Screen.virtualY + Screen.desktopAvailableHeight - height - 16)) : ownerWindow.y + anchorCenter.y - height / 2
    DialogShell { id: shell; anchors.fill: parent; model: dialog.model }
    onClosing: event => { event.accepted = false; model.dispatch("cancel") }
}
