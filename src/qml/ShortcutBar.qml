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
        visible: !root.macroMode && root.page.state.hasSequenceBindings
        Layout.fillWidth: true
        spacing: 6
        QuietLabel { text: qsTr("当前段"); color: Theme.muted; wrapMode: Text.NoWrap }
        Item {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            implicitHeight: sequenceText.implicitHeight
            HoverHandler { id: sequenceHover }
            QuietLabel {
                id: sequenceText
                anchors.fill: parent
                text: root.page.state.sequenceSelectionText
                elide: Text.ElideRight
                wrapMode: Text.NoWrap
                Accessible.name: qsTr("当前切换段：%1").arg(text)
            }
            QuietToolTip {
                text: root.page.state.sequenceSelectionText
                visible: sequenceHover.hovered
            }
        }
    }
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
                editable: !root.macroMode && root.page.state.hasSequenceKey
                editHint: qsTr("编辑切换键")
                enabled: !root.page.state.running
                onEditRequested: root.page.sequenceSwitchKeyAddClicked()
                onRemoveRequested: root.page.sequenceSwitchKeyDeleteClicked()
            }
            IconButton {
                compact: true
                iconName: "add"
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
            IconButton { compact: true; iconName: "add"; hint: qsTr("添加暂停键"); enabled: !root.page.state.running; onClicked: root.page.pauseKeyAddClicked() }
            Item { Layout.fillWidth: true; Layout.preferredWidth: 0 }
        }
    }
}
