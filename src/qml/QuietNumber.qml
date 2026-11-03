import QtQuick
import QtQuick.Controls.Basic
import XiaoJi.UI

SpinBox {
    id: control
    implicitHeight: Theme.buttonHeight
    implicitWidth: 136
    from: 0
    to: 2147483647
    editable: true
    font.family: Theme.fontFamily
    font.pixelSize: Theme.bodyFontSize
    font.hintingPreference: Theme.fontHintingPreference
    hoverEnabled: true
    palette.text: Theme.text
    palette.base: Theme.field
    palette.buttonText: Theme.text
    palette.button: Theme.field
    palette.mid: Theme.accentSoft
    palette.highlight: Theme.accent
    palette.highlightedText: Theme.accentForeground
    contentItem: TextInput {
        text: control.textFromValue(control.value, control.locale)
        font: control.font
        renderType: Theme.textRenderType
        color: Theme.text
        selectionColor: Theme.accent
        selectedTextColor: Theme.accentForeground
        horizontalAlignment: Qt.AlignHCenter
        verticalAlignment: Qt.AlignVCenter
        readOnly: !control.editable
        validator: control.validator
        inputMethodHints: Qt.ImhDigitsOnly
    }
    background: Rectangle {
        color: Theme.field
        radius: Theme.inputRadius
        border.color: control.visualFocus ? Theme.accent : control.enabled && control.hovered ? Theme.accentBorder : Theme.border
        border.width: control.visualFocus ? 2 : 1
    }
}
