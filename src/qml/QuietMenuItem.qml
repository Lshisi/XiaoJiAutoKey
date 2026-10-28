import QtQuick
import QtQuick.Controls.Basic
import XiaoJi.UI

MenuItem {
    id: control
    implicitHeight: Math.max(36, contentItem.implicitHeight + 12)
    font.family: Theme.fontFamily
    font.pixelSize: 12
    contentItem: Text {
        text: control.text
        textFormat: Text.PlainText
        font: control.font
        color: control.enabled ? Theme.text : Theme.muted
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
    background: Rectangle {
        radius: Theme.buttonRadius
        color: control.highlighted ? Theme.hover : "transparent"
        border.width: control.visualFocus ? 1 : 0
        border.color: Theme.accent
    }
}
