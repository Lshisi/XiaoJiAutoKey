import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import XiaoJi.UI

ColumnLayout {
    id: processes
    property string query: ""
    readonly property var fields: dialogModel.dialogState
    readonly property var matches: fields.runningProcesses.filter(name => name.indexOf(query.toLowerCase()) >= 0)
    spacing: 12
    QuietLabel { Layout.fillWidth: true; text: qsTr("已内置识别 JX3Client 系列客户端。额外进程名支持 name* 前缀通配。"); color: Theme.muted }
    QuietTextField { Layout.fillWidth: true; placeholderText: qsTr("搜索正在运行的进程"); onTextEdited: processes.query = text; Accessible.name: placeholderText }
    RowLayout {
        Layout.fillWidth: true
        QuietCombo {
            Layout.fillWidth: true
            model: [qsTr("从运行中的进程选择…")].concat(processes.matches)
            currentIndex: 0
            onActivated: { if (currentIndex > 0) dialogModel.dispatch("selectProcess", currentText) }
            Accessible.name: qsTr("运行中的进程")
        }
        IconButton { glyph: "\uE72C"; hint: qsTr("刷新进程列表"); onClicked: dialogModel.dispatch("refreshProcesses") }
    }
    RowLayout {
        Layout.fillWidth: true
        QuietTextField { Layout.fillWidth: true; text: processes.fields.processName; placeholderText: qsTr("手动输入进程名，例如 mumu*"); maximumLength: 260; onTextEdited: dialogModel.edit("processName", text); onAccepted: dialogModel.dispatch("add"); Accessible.name: placeholderText }
        ActionButton { text: qsTr("添加"); onClicked: dialogModel.dispatch("add") }
    }
    ListView {
        id: list
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.minimumHeight: 120
        Layout.preferredHeight: 140
        clip: true
        model: processes.fields.processes
        delegate: RowLayout {
            id: processRow
            required property string modelData
            required property int index
            width: list.width - 12
            height: 40
            QuietLabel { Layout.fillWidth: true; Layout.minimumWidth: 0; text: processRow.modelData; elide: Text.ElideRight; wrapMode: Text.NoWrap }
            IconButton { glyph: "\uE74D"; hint: qsTr("删除进程 %1").arg(processRow.modelData); onClicked: dialogModel.dispatch("remove", processRow.index) }
        }
        ScrollBar.vertical: QuietScrollBar {}
    }
}
