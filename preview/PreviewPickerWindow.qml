import QtQuick
import XiaoJi.UI

Window {
    required property Window ownerWindow
    transientParent: ownerWindow
    modality: Qt.ApplicationModal
    flags: Qt.Dialog | Qt.FramelessWindowHint
    visible: true
    color: "transparent"
    x: ownerWindow.x
    y: ownerWindow.y
    width: ownerWindow.width
    height: ownerWindow.height
    title: qsTr("坐标拾取界面预览")
    PointPicker { anchors.fill: parent }
    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        focus: true
        onPositionChanged: mouse => appView.setPickerPoint(mouse.x, mouse.y)
        onClicked: appView.endPicking()
        Keys.onEscapePressed: appView.endPicking()
    }
    onClosing: event => { event.accepted = false; appView.endPicking() }
}
