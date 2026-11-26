import QtQuick
import QtQuick.Controls.Basic
import XiaoJi.UI

Switch {
    id: control
    CrispyControlFeedback { targetControl: control }
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
            ? (control.down ? Theme.primaryPressed : control.hovered ? Theme.primaryHover : Theme.primaryFill)
            : control.down ? Theme.accentSoft : control.hovered ? Theme.accentSubtle : Theme.hover
        border.color: control.visualFocus ? Theme.accent : control.enabled && control.hovered ? Theme.accentBorder : Theme.border
        border.width: control.visualFocus ? 2 : 1
        Rectangle {
            x: 4 + control.visualPosition * (parent.width - width - 8)
            y: (parent.height - height) / 2
            width: 12
            height: 12
            radius: 6
            color: !control.enabled ? Theme.muted : control.checked ? Theme.primaryForeground : Theme.text
            Behavior on x {
                enabled: Theme.motionEnabled && control.visible && !control.down
                NumberAnimation { duration: Theme.toggleDuration; easing.type: Easing.OutCubic }
            }
        }
    }
}
