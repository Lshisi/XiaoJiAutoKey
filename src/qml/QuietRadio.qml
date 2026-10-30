import QtQuick
import QtQuick.Controls.Basic
import XiaoJi.UI

RadioButton {
    id: control
    implicitHeight: Math.max(22, contentItem.implicitHeight + 4)
    spacing: 8
    padding: 2
    font.family: Theme.fontFamily
    font.pixelSize: 12
    hoverEnabled: true
    indicator: Rectangle {
        implicitWidth: 14
        implicitHeight: 14
        x: control.leftPadding
        y: (control.height - height) / 2
        radius: 7
        color: !control.enabled ? Theme.hover : control.down ? Theme.accentSoft : control.hovered ? Theme.accentSubtle : Theme.field
        border.color: !control.enabled ? Theme.border : control.checked || control.visualFocus ? Theme.accent : control.hovered ? Theme.accentBorder : Theme.border
        border.width: control.visualFocus ? 2 : 1
        Rectangle {
            anchors.centerIn: parent
            width: 6
            height: 6
            radius: 3
            visible: control.checked
            color: control.enabled ? Theme.accent : Theme.muted
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
