import QtQuick
import QtQuick.Controls.Basic
import XiaoJi.UI

Button {
    id: control
    property bool primary: false
    property bool quiet: false
    property bool destructive: false
    property string hint: ""
    implicitHeight: Math.max(Theme.buttonHeight, contentItem.implicitHeight + topPadding + bottomPadding)
    implicitWidth: Math.max(implicitHeight, contentItem.implicitWidth + leftPadding + rightPadding)
    leftPadding: 12
    rightPadding: 12
    topPadding: 6
    bottomPadding: 6
    font.family: Theme.fontFamily
    font.pixelSize: 12
    hoverEnabled: true
    focusPolicy: Qt.TabFocus
    Accessible.name: text.length ? text : hint
    contentItem: Text {
        text: control.text
        textFormat: Text.PlainText
        font: control.font
        color: !control.enabled ? Theme.muted : control.destructive ? (Theme.dark ? Theme.background : Theme.field)
            : control.primary ? Theme.accentForeground : Theme.text
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
    background: Rectangle {
        radius: Theme.buttonRadius
        color: !control.enabled ? Theme.hover : control.destructive ? (control.down || control.hovered ? Qt.darker(Theme.danger, 1.1) : Theme.danger) : control.primary
            ? (control.down ? Theme.accentPressed : control.hovered ? Theme.accentHover : Theme.accent)
            : control.down || control.hovered ? Theme.hover : control.quiet ? "transparent" : Theme.field
        border.width: control.visualFocus ? 2 : control.primary || control.quiet ? 0 : 1
        border.color: control.visualFocus ? Theme.accent : Theme.border
    }
}
