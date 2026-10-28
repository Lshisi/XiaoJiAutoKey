import QtQuick
import QtQuick.Controls.Basic
import XiaoJi.UI

ScrollBar {
    id: control
    implicitWidth: 8
    implicitHeight: 8
    padding: 1
    minimumSize: 0.06
    contentItem: Rectangle {
        implicitWidth: 6
        implicitHeight: 6
        radius: 3
        color: control.hovered || control.pressed ? Theme.colors.scrollHandleHover : Theme.colors.scrollHandle
        visible: control.size < 1
    }
    background: Item {}
}
