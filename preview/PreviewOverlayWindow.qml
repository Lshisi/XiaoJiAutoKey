import QtQuick
import XiaoJi.UI

Window {
    id: overlay
    required property Window ownerWindow
    transientParent: ownerWindow
    flags: Qt.Tool | Qt.FramelessWindowHint
    visible: true
    color: "transparent"
    title: qsTr("悬浮窗预览")
    width: 178
    height: content.implicitHeight
    x: ownerWindow.x + ownerWindow.width - width - 16
    y: ownerWindow.y + Theme.titleBarHeight + 24
    OverlayWindow { id: content; anchors.fill: parent }
    MouseArea {
        anchors.fill: parent
        enabled: !overlayModel.overlayState.locked
        onPressed: overlay.startSystemMove()
        onDoubleClicked: overlay.ownerWindow.requestActivate()
        Accessible.ignored: true
    }
    OverlayLock { anchors.top: parent.top; anchors.right: parent.right; anchors.margins: 4 }
}
