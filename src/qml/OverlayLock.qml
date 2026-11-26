import QtQuick
import QtQuick.Controls.Basic
import XiaoJi.UI

Button {
    id: control
    CrispyControlFeedback { targetControl: control }
    readonly property var appearance: overlayModel.overlayState
    implicitWidth: 32
    implicitHeight: 32
    focusPolicy: Qt.NoFocus
    hoverEnabled: true
    Accessible.name: appearance.locked ? qsTr("解锁悬浮窗位置") : qsTr("锁定悬浮窗位置并开启鼠标穿透")
    onClicked: overlayModel.toggleLock()
    contentItem: QuietIcon {
        name: control.appearance.locked ? "lock" : "unlock"
        pixelSize: 12
        color: control.appearance.text
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
