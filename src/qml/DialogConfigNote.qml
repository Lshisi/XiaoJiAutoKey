import QtQuick
import QtQuick.Layouts
import XiaoJi.UI

ColumnLayout {
    id: editor
    readonly property var fields: dialogModel.dialogState
    spacing: Theme.cardGap
    QuietLabel {
        Layout.fillWidth: true
        text: editor.fields.name
        font.weight: Theme.headingWeight
        wrapMode: Text.WrapAnywhere
    }
    QuietTextArea {
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.minimumHeight: 100
        implicitHeight: 140
        text: dialogModel.dialogState.note
        placeholderText: qsTr("写下适用场景、游戏内键位和操作说明…")
        onEdited: value => dialogModel.edit("note", value)
        Accessible.name: qsTr("配置备注")
    }
    QuietLabel {
        Layout.fillWidth: true
        text: qsTr("备注会随配置复制和分享；清空后保存即可移除。")
        color: Theme.muted
    }
}
