import QtQuick
import QtQuick.Controls.Basic

Button {
    id: control
    readonly property var appearance: overlayModel.overlayState
    implicitWidth: 32
    implicitHeight: 32
    focusPolicy: Qt.NoFocus
    hoverEnabled: true
    text: appearance.locked ? "\uE72E" : "\uE785"
    font.family: "Segoe MDL2 Assets"
    font.pixelSize: 12
    Accessible.name: appearance.locked ? qsTr("解锁悬浮窗位置") : qsTr("锁定悬浮窗位置并开启鼠标穿透")
    onClicked: overlayModel.toggleLock()
    contentItem: Text {
        text: control.text
        font: control.font
        color: control.appearance.text
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }
    background: Rectangle {
        radius: 8
        color: control.down ? Qt.alpha(control.appearance.accent, 0.24)
            : control.hovered ? Qt.alpha(control.appearance.accent, 0.15)
            : "transparent"
        border.color: control.hovered || control.down ? control.appearance.accentBorder
            : "transparent"
    }
}
