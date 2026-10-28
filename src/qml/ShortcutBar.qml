import QtQuick
import QtQuick.Layouts
import XiaoJi.UI

ColumnLayout {
    id: root
    required property QtObject page
    property bool macroMode: false
    spacing: 10
    Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Theme.divider }
    Flow {
        Layout.fillWidth: true
        spacing: 8
        Row {
            visible: !root.macroMode
            spacing: 6
            QuietLabel { text: qsTr("切换键"); color: Theme.muted; font.pixelSize: 12; height: Theme.iconSize; verticalAlignment: Text.AlignVCenter }
            ActionButton {
                text: root.macroMode ? "" : root.page.state.sequenceKey
                enabled: !root.page.state.running
                quiet: true
                onClicked: root.page.sequenceSwitchKeyAddClicked()
            }
            IconButton {
                visible: !root.macroMode && root.page.state.hasSequenceKey
                glyph: "\uE711"
                hint: qsTr("清除切换键")
                enabled: !root.page.state.running
                onClicked: root.page.sequenceSwitchKeyDeleteClicked()
            }
        }
        QuietLabel { text: qsTr("暂停键"); color: Theme.muted; font.pixelSize: 12; height: Theme.iconSize; verticalAlignment: Text.AlignVCenter }
        Repeater {
            model: root.page.state.pauseKeys
            delegate: ActionButton {
                required property string modelData
                required property int index
                text: qsTr("%1 ×").arg(modelData)
                hint: qsTr("删除暂停键 %1").arg(modelData)
                implicitHeight: Theme.iconSize
                enabled: !root.page.state.running
                onClicked: root.page.pauseKeyDeleteClicked(index)
            }
        }
        IconButton { glyph: "\uE710"; hint: qsTr("添加暂停键"); enabled: !root.page.state.running; onClicked: root.page.pauseKeyAddClicked() }
    }
}
