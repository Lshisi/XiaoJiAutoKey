import QtQuick
import QtQuick.Controls.Basic
import XiaoJi.UI

Slider {
    id: control
    implicitHeight: 32
    from: 0
    to: 100
    stepSize: 1
    hoverEnabled: true
    background: Rectangle {
        x: control.leftPadding
        y: (control.height - height) / 2
        width: control.availableWidth
        height: 4
        radius: 2
        color: Theme.border
        Rectangle { width: control.visualPosition * parent.width; height: parent.height; radius: 2; color: Theme.accent }
    }
    handle: Rectangle {
        x: control.leftPadding + control.visualPosition * (control.availableWidth - width)
        y: (control.height - height) / 2
        implicitWidth: 18
        implicitHeight: 18
        radius: 9
        color: control.enabled && control.pressed ? Theme.accentSoft : control.enabled && control.hovered ? Theme.accentSubtle : Theme.field
        border.color: control.visualFocus || (control.enabled && control.pressed) ? Theme.accent : control.enabled && control.hovered ? Theme.accentBorder : Theme.border
        border.width: control.visualFocus ? 2 : 1
    }
}
