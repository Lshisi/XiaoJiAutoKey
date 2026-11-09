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
}
