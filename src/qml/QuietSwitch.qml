import QtQuick
import QtQuick.Controls.Basic
import XiaoJi.UI

Switch {
    id: control
    implicitWidth: 36
    implicitHeight: 24
    padding: 0
    hoverEnabled: true
    indicator: Rectangle {
        implicitWidth: 36
        implicitHeight: 20
        y: (control.height - height) / 2
        radius: 10
        color: !control.enabled ? Theme.hover : control.checked
            ? (control.down ? Theme.accentPressed : control.hovered ? Theme.accentHover : Theme.accent)
            : control.down ? Theme.accentSoft : control.hovered ? Theme.accentSubtle : Theme.hover
        border.color: control.visualFocus ? Theme.accent : control.enabled && control.hovered ? Theme.accentBorder : Theme.border
        border.width: control.visualFocus ? 2 : 1
        Rectangle {
            x: control.checked ? parent.width - width - 4 : 4
            y: (parent.height - height) / 2
            width: 12
            height: 12
            radius: 6
            color: !control.enabled ? Theme.muted : control.checked ? Theme.accentForeground : Theme.text
        }
    }
}
