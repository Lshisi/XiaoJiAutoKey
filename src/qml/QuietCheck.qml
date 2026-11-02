import QtQuick
import QtQuick.Controls.Basic
import XiaoJi.UI

CheckBox {
    id: control
    implicitHeight: Math.max(22, contentItem.implicitHeight + 4)
    spacing: 8
    padding: 2
    font.family: Theme.fontFamily
    font.pixelSize: Theme.bodyFontSize
    hoverEnabled: true
    indicator: Rectangle {
        implicitWidth: 16
        implicitHeight: 16
        x: control.leftPadding
        y: Math.round((control.height - height) / 2)
        radius: 4
        color: !control.enabled ? Theme.hover : control.checked
            ? (control.down ? Theme.accentPressed : control.hovered ? Theme.accentHover : Theme.accent)
            : control.down ? Theme.accentSoft : control.hovered ? Theme.accentSubtle : Theme.field
        border.width: control.visualFocus ? 2 : 1
        border.color: !control.enabled ? Theme.border : control.visualFocus || control.checked ? Theme.accent : control.hovered ? Theme.accentBorder : Theme.border
        QuietIcon {
            anchors.centerIn: parent
            name: "check"
            visible: control.checked
            color: control.enabled ? Theme.accentForeground : Theme.muted
            pixelSize: Theme.iconGlyphSize
        }
    }
    contentItem: QuietLabel {
        text: control.text
        font: control.font
        leftPadding: control.indicator.width + control.spacing
        color: control.enabled ? Theme.text : Theme.muted
        verticalAlignment: Text.AlignVCenter
    }
}
