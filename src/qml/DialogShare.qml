import QtQuick
import QtQuick.Layouts
import XiaoJi.UI

ColumnLayout {
    id: sharing
    readonly property var fields: dialogModel.dialogState
    readonly property bool formatVisible: sharing.fields.formatVisible === undefined
        ? sharing.fields.exporting : sharing.fields.formatVisible
    spacing: 12
    QuietCombo {
        visible: sharing.formatVisible
        Layout.fillWidth: true
        model: [qsTr("分享码（XJK1:…，适合发送）"), qsTr("文本配方（可读，适合论坛正文）")]
        currentIndex: sharing.fields.format
        onActivated: dialogModel.edit("format", currentIndex)
        Accessible.name: qsTr("导出格式")
    }
    QuietTextArea {
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.minimumHeight: 120
        implicitHeight: 160
        text: sharing.fields.code
        readOnly: sharing.fields.exporting
        placeholderText: qsTr("粘贴分享码（XJK1:…）或文本配方，识别后确认导入")
        onEdited: value => dialogModel.edit("code", value)
        Accessible.name: sharing.fields.diagnostic
            ? qsTr("诊断信息内容")
            : (sharing.fields.exporting ? qsTr("导出的配置内容") : qsTr("要导入的配置内容"))
    }
    QuietLabel { Layout.fillWidth: true; text: sharing.fields.status; color: Theme.muted; font.pixelSize: 12 }
    QuietLabel {
        Layout.fillWidth: true
        visible: sharing.fields.namePreview.length > 0
        text: qsTr("按键名称")
        color: Theme.muted
    }
    QuietTextArea {
        Layout.fillWidth: true
        visible: sharing.fields.namePreview.length > 0
        implicitHeight: 72
        text: sharing.fields.namePreview
        readOnly: true
        Accessible.name: qsTr("分享配置中的按键名称")
    }
}
