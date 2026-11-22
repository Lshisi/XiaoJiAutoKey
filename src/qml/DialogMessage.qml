import QtQuick
import QtQuick.Layouts
import XiaoJi.UI

ColumnLayout {
    spacing: 12
    QuietLabel {
        Layout.fillWidth: true
        text: dialogModel.dialogState.message
        color: Theme.bodyText
    }
    QuietCheck {
        Layout.fillWidth: true
        visible: !!dialogModel.dialogState.optionText
        text: dialogModel.dialogState.optionText || ""
        checked: dialogModel.dialogState.optionChecked === true
        onClicked: dialogModel.edit("optionChecked", checked)
    }
}
