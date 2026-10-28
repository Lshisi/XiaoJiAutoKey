import QtQuick
import QtQuick.Controls.Basic
import XiaoJi.UI

Item {
    id: context
    required property var textControl
    property point menuPosition
    signal modified()
    anchors.fill: parent
    Accessible.ignored: true

    function openAt(position) {
        menuPosition = position;
        if (menuLoader.status === Loader.Ready)
            menuLoader.item.popup(context, position);
        else
            menuLoader.active = true;
    }
    TapHandler {
        acceptedButtons: Qt.RightButton
        onTapped: eventPoint => context.openAt(eventPoint.position)
    }
    Loader {
        id: menuLoader
        active: false
        onLoaded: { if (status === Loader.Ready) item.popup(context, context.menuPosition) }
        sourceComponent: Menu {
            popupType: Popup.Item
            padding: 4
            width: 152
            margins: 8
            onClosed: menuLoader.active = false
            background: Rectangle { color: Theme.surface; border.color: Theme.border; radius: Theme.inputRadius }
            QuietMenuItem { text: qsTr("剪切"); enabled: !context.textControl.readOnly && context.textControl.selectedText.length > 0; onTriggered: { context.textControl.cut(); context.modified() } }
            QuietMenuItem { text: qsTr("复制"); enabled: context.textControl.selectedText.length > 0; onTriggered: context.textControl.copy() }
            QuietMenuItem { text: qsTr("粘贴"); enabled: !context.textControl.readOnly && context.textControl.canPaste; onTriggered: { context.textControl.paste(); context.modified() } }
            QuietMenuItem { text: qsTr("全选"); enabled: context.textControl.length > 0; onTriggered: context.textControl.selectAll() }
        }
    }
}
