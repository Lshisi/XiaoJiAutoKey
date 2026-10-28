import QtQuick
import QtQuick.Controls.Basic
import XiaoJi.UI

IconButton {
    id: control
    property bool closeAction: false
    glyph: closeAction ? "\uE711" : "\uE921"
    hint: closeAction ? qsTr("关闭") : qsTr("最小化")
    implicitWidth: 40
    font.pixelSize: 10
    contentItem: Text {
        text: control.glyph
        font: control.font
        color: control.closeAction && (control.hovered || control.down)
            ? (Theme.dark ? Theme.background : Theme.field) : Theme.text
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        Accessible.ignored: true
    }
    background: Rectangle {
        radius: Theme.buttonRadius
        color: control.down || control.hovered
            ? (control.closeAction ? (control.down ? Qt.darker(Theme.danger, 1.1) : Theme.danger) : (control.down ? Theme.border : Theme.hover))
            : "transparent"
        border.width: control.visualFocus ? 2 : 0
        border.color: Theme.accent
    }
}
