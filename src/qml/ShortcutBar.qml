import QtQuick
import QtQuick.Layouts
import XiaoJi.UI

ColumnLayout {
    id: root
    required property QtObject page
    property bool macroMode: false
    spacing: 8
    Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Theme.divider }
    RowLayout {
        Layout.fillWidth: true
        spacing: Theme.formGap
        RowLayout {
            visible: !root.macroMode
            Layout.fillWidth: false
            spacing: 6
            QuietLabel { text: qsTr("切换键"); color: Theme.muted; wrapMode: Text.NoWrap }
            ShortcutTag {
                Layout.minimumWidth: implicitWidth
                Layout.maximumWidth: implicitWidth
                text: root.macroMode ? "" : root.page.state.sequenceKey
                removable: !root.macroMode && root.page.state.hasSequenceKey
                removeHint: qsTr("清除切换键")
                enabled: !root.page.state.running
                onRemoveRequested: root.page.sequenceSwitchKeyDeleteClicked()
            }
            IconButton {
                compact: true
                glyph: "\uE710"
                hint: qsTr("设置切换键")
                enabled: !root.page.state.running
                onClicked: root.page.sequenceSwitchKeyAddClicked()
            }
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: 6
            QuietLabel { text: qsTr("暂停键"); color: Theme.muted; wrapMode: Text.NoWrap }
            PauseKeyList {
                id: pauseKeys
                Layout.fillWidth: true
                Layout.minimumWidth: pauseKeys.keys.length > 0 ? pauseKeys.moreWidth : implicitWidth
                Layout.maximumWidth: implicitWidth
                page: root.page
            }
            IconButton { compact: true; glyph: "\uE710"; hint: qsTr("添加暂停键"); enabled: !root.page.state.running; onClicked: root.page.pauseKeyAddClicked() }
            Item { Layout.fillWidth: true; Layout.preferredWidth: 0 }
        }
    }
}
