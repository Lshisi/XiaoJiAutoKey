import QtQuick
import QtQuick.Layouts
import XiaoJi.UI

ColumnLayout {
    spacing: 10
    QuietLabel { Layout.fillWidth: true; text: qsTr("关闭程序，还是最小化到系统托盘？") }
    QuietCheck { text: qsTr("记住我的选择"); checked: dialogModel.dialogState.remember; onClicked: dialogModel.edit("remember", checked) }
}
