import QtQuick
import QtQuick.Controls.Basic
import XiaoJi.UI

CheckBox {
    id: control
    implicitHeight: Math.max(22, contentItem.implicitHeight + 4)
    spacing: 8
    padding: 2
    font.family: Theme.fontFamily
    font.pixelSize: 12
    hoverEnabled: true
    indicator: Rectangle {
        implicitWidth: 16
        implicitHeight: 16
        x: control.leftPadding
        y: Math.round((control.height - height) / 2)
        radius: 4
        color: control.checked ? Theme.accent : Theme.field
        border.width: control.visualFocus ? 2 : 1
        border.color: control.visualFocus || control.checked ? Theme.accent : Theme.border
        Text {
            anchors.centerIn: parent
            text: control.checked ? "✓" : ""
            color: Theme.accentForeground
            font.family: "Segoe UI Symbol"
            font.pixelSize: 12
            Accessible.ignored: true
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
