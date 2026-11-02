import QtQuick
import QtQuick.Layouts
import XiaoJi.UI

ColumnLayout {
    id: sound
    readonly property var fields: dialogModel.dialogState
    spacing: 10
    Repeater {
        model: [{ label: qsTr("开启音效"), running: true, field: "soundOn" }, { label: qsTr("关闭音效"), running: false, field: "soundOff" }]
        delegate: RowLayout {
            id: soundRow
            required property var modelData
            readonly property string path: sound.fields[modelData.field]
            Layout.fillWidth: true
            spacing: 8
            QuietLabel { text: soundRow.modelData.label }
            ActionButton {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                text: soundRow.path.length ? soundRow.path : qsTr("默认内置音效")
                hint: qsTr("选择自定义音效")
                onClicked: dialogModel.dispatch("chooseSound", soundRow.modelData.running)
            }
            IconButton { iconName: "play"; hint: qsTr("试听音效"); onClicked: dialogModel.dispatch("previewSound", soundRow.modelData.running) }
            IconButton { iconName: "delete"; hint: qsTr("恢复内置音效"); onClicked: dialogModel.dispatch("resetSound", soundRow.modelData.running) }
        }
    }
    RowLayout {
        Layout.fillWidth: true
        QuietLabel { text: qsTr("音量") }
        QuietSlider { Layout.fillWidth: true; value: sound.fields.volume; onMoved: dialogModel.edit("volume", Math.round(value)); Accessible.name: qsTr("提示音音量") }
        QuietLabel { text: qsTr("%1%").arg(sound.fields.volume); color: Theme.muted }
    }
    QuietLabel { Layout.fillWidth: true; text: qsTr("支持 mp3、wav、wma、mid；清空路径后使用内置音效。"); color: Theme.muted }
    Item { Layout.fillHeight: true }
}
