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
        spacing: 6
        QuietLabel { visible: !root.macroMode; text: qsTr("切换键:"); color: Theme.muted }
        ShortcutTag {
            visible: !root.macroMode
            Layout.minimumWidth: implicitWidth
            Layout.maximumWidth: implicitWidth
            text: root.macroMode ? "" : root.page.state.sequenceKey
            removable: !root.macroMode && root.page.state.hasSequenceKey
            removeHint: qsTr("清除切换键")
            enabled: !root.page.state.running
            onRemoveRequested: root.page.sequenceSwitchKeyDeleteClicked()
        }
        IconButton {
            visible: !root.macroMode
            compact: true
            glyph: "\uE710"
            hint: qsTr("设置切换键")
            enabled: !root.page.state.running && !root.page.state.sequenceRecording
            onClicked: root.page.sequenceSwitchKeyAddClicked()
        }
        Rectangle { visible: !root.macroMode; Layout.preferredWidth: 1; Layout.preferredHeight: 18; color: Theme.divider }
        QuietLabel { text: qsTr("暂停键:"); color: Theme.muted }
        PauseKeyList { Layout.fillWidth: true; Layout.minimumWidth: implicitWidth; page: root.page }
        IconButton { compact: true; glyph: "\uE710"; hint: qsTr("添加暂停键"); enabled: !root.page.state.running; onClicked: root.page.pauseKeyAddClicked() }
    }
}
