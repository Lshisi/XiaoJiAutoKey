import QtQuick
import QtQuick.Layouts
import XiaoJi.UI

ColumnLayout {
    spacing: 12
    QuietLabel { Layout.fillWidth: true; text: dialogModel.dialogState.message; color: Theme.muted }
    QuietTextField {
        Layout.fillWidth: true
        text: dialogModel.dialogState.name
        Accessible.name: qsTr("配置名称")
        onTextEdited: dialogModel.edit("name", text)
        onAccepted: dialogModel.dispatch("accept")
        Component.onCompleted: { forceActiveFocus(); selectAll() }
    }
}
