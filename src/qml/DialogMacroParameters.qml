import QtQuick
import QtQuick.Layouts
import XiaoJi.UI

ColumnLayout {
    id: root
    required property var fields
    required property real labelWidth
    spacing: Theme.formGap
    RowLayout {
        visible: root.fields.action === 1 || root.fields.action === 4
        Layout.fillWidth: true
        spacing: 12
        QuietLabel {
            Layout.minimumWidth: root.labelWidth
            Layout.maximumWidth: root.labelWidth
            text: root.fields.action === 1 ? qsTr("保持(ms):") : qsTr("等待(ms):")
        }
        QuietTextField {
            Layout.fillWidth: true
            text: root.fields.duration
            validator: IntValidator { bottom: 1; top: 2147483647 }
            inputMethodHints: Qt.ImhDigitsOnly
            onTextEdited: dialogModel.edit("duration", text)
            Accessible.name: qsTr("动作时长，毫秒")
        }
    }
    RowLayout {
        visible: root.fields.action === 5
        Layout.fillWidth: true
        spacing: 12
        QuietLabel { Layout.minimumWidth: root.labelWidth; Layout.maximumWidth: root.labelWidth; text: qsTr("滚轮:") }
        QuietCombo {
            Layout.fillWidth: true
            model: [qsTr("向上"), qsTr("向下")]
            currentIndex: root.fields.wheelDirection
            onActivated: dialogModel.edit("wheelDirection", currentIndex)
            Accessible.name: qsTr("滚轮方向")
        }
        QuietTextField {
            Layout.preferredWidth: 70
            text: root.fields.wheelCount
            validator: IntValidator { bottom: 1; top: 100 }
            onTextEdited: dialogModel.edit("wheelCount", text)
            Accessible.name: qsTr("滚轮格数")
        }
        QuietLabel { text: qsTr("格") }
    }
}
