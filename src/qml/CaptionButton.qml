import QtQuick
import QtQuick.Controls.Basic
import XiaoJi.UI

IconButton {
    id: control
    property bool closeAction: false
    iconName: closeAction ? "close" : "minimize"
    hint: closeAction ? qsTr("关闭") : qsTr("最小化")
    implicitWidth: 40
    iconPixelSize: Theme.smallIconGlyphSize
    contentItem: QuietIcon {
        name: control.iconName
        pixelSize: control.iconPixelSize
        color: control.closeAction && (control.hovered || control.down)
            ? (Theme.dark ? Theme.background : Theme.field) : Theme.text
    }
    background: Rectangle {
        radius: Theme.buttonRadius
        color: control.down || control.hovered
            ? (control.closeAction ? (control.down ? Qt.darker(Theme.danger, 1.1) : Theme.danger) : (control.down ? Theme.accentSoft : Theme.accentSubtle))
            : "transparent"
        border.width: control.visualFocus ? 2 : 0
        border.color: Theme.accent
    }
}
