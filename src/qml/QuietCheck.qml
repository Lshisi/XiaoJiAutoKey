import QtQuick
import QtQuick.Controls.Basic
import XiaoJi.UI

CheckBox {
    id: control
    CrispyControlFeedback { targetControl: control }
    implicitHeight: Math.max(22, contentItem.implicitHeight + 4)
    spacing: 8
    padding: 2
    font.family: Theme.fontFamily
    font.pixelSize: Theme.bodyFontSize
    font.weight: Theme.bodyWeight
    font.hintingPreference: Theme.fontHintingPreference
    hoverEnabled: true
    indicator: Rectangle {
        implicitWidth: 16
        implicitHeight: 16
        x: control.leftPadding
        y: Math.round((control.height - height) / 2)
        radius: 4
        color: !control.enabled ? Theme.hover : control.checked
            ? (control.down ? Theme.primaryPressed : control.hovered ? Theme.primaryHover : Theme.primaryFill)
            : control.down ? Theme.accentSoft : control.hovered ? Theme.accentSubtle : Theme.field
        border.width: control.visualFocus ? 2 : 1
        border.color: !control.enabled ? Theme.border : control.visualFocus ? Theme.accent : control.checked ? Theme.primaryFill : control.hovered ? Theme.accentBorder : Theme.border
        QuietIcon {
            anchors.centerIn: parent
            name: "check"
            visible: control.checked
            color: control.enabled ? Theme.primaryForeground : Theme.muted
            pixelSize: Theme.iconGlyphSize
        }
    }
    contentItem: QuietLabel {
        text: control.text
        font: control.font
        leftPadding: control.indicator.width + control.spacing
        color: control.enabled ? Theme.bodyText : Theme.muted
        verticalAlignment: Text.AlignVCenter
    }
}
