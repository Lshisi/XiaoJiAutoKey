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
    font.pixelSize: 13
    palette.text: Theme.text
    palette.base: Theme.field
    palette.buttonText: Theme.text
    palette.button: Theme.field
    contentItem: TextInput {
        text: control.textFromValue(control.value, control.locale)
        font: control.font
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
        border.color: control.visualFocus ? Theme.accent : Theme.border
        border.width: control.visualFocus ? 2 : 1
    }
}
