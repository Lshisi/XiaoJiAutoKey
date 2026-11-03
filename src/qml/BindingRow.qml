import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import XiaoJi.UI

Item {
    id: root
    required property QtObject page
    required property var entry
    required property QtObject columns
    readonly property bool selectionActive: root.page.state.selectedBindingIds.length > 0
    readonly property bool selected: root.page.state.selectedBindingIds.indexOf(root.entry.id) >= 0
    implicitHeight: Theme.tableRowHeight
    HoverHandler { id: rowHover }
    MouseArea {
        anchors.fill: parent
        onDoubleClicked: {
            if (!root.selectionActive) root.page.editBinding(root.entry.id)
        }
    }
    Rectangle {
        anchors.fill: parent
        color: root.selected ? Theme.accentSoft
            : root.enabled && rowHover.hovered ? Theme.accentSubtle : "transparent"
    }
    Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Theme.divider }
    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: root.columns.contentLeftPadding
        anchors.rightMargin: root.columns.contentRightPadding
        spacing: 0
        Item {
            Layout.minimumWidth: Theme.compactIconSize
            Layout.maximumWidth: Theme.compactIconSize
            Layout.fillHeight: true
            QuietCheck {
                anchors.centerIn: parent
                spacing: 0
                checked: root.selected
                Accessible.name: qsTr("选择 %1").arg(root.entry.keyDisplayText)
                onClicked: root.page.toggleBindingSelection(root.entry.id)
            }
        }
        KeyCell {
            Layout.minimumWidth: root.columns.keyWidth
            Layout.maximumWidth: root.columns.keyWidth
            text: root.entry.keyText
            description: root.entry.positionHint
        }
        NameCell { Layout.minimumWidth: root.columns.nameWidth; Layout.maximumWidth: root.columns.nameWidth; name: root.entry.name }
        QuietLabel { Layout.minimumWidth: root.columns.modeWidth; Layout.maximumWidth: root.columns.modeWidth; text: root.entry.modeText; horizontalAlignment: Text.AlignHCenter; elide: Text.ElideRight; wrapMode: Text.NoWrap }
        QuietLabel {
            Layout.minimumWidth: root.columns.priorityWidth
            Layout.maximumWidth: root.columns.priorityWidth
            text: root.entry.priorityText
            horizontalAlignment: Text.AlignHCenter
            color: root.entry.priorityKind === "enabled" ? Theme.text : Theme.muted
        }
        QuietLabel { Layout.minimumWidth: root.columns.delayWidth; Layout.maximumWidth: root.columns.delayWidth; text: root.entry.delay.toString(); horizontalAlignment: Text.AlignHCenter; elide: Text.ElideRight; wrapMode: Text.NoWrap }
        WorkspaceEnabledCell {
            Layout.minimumWidth: root.columns.statusWidth
            Layout.maximumWidth: root.columns.statusWidth
            Layout.fillHeight: true
            page: root.page
            entry: root.entry
        }
        Item {
            Layout.minimumWidth: root.columns.actionWidth
            Layout.maximumWidth: root.columns.actionWidth
            Layout.fillHeight: true
            Row {
                anchors.centerIn: parent
                visible: !root.selectionActive
                spacing: 0
                IconButton { iconName: "edit"; hint: qsTr("编辑 %1").arg(root.entry.keyDisplayText); onClicked: root.page.editBinding(root.entry.id) }
                IconButton { iconName: "delete"; hint: qsTr("删除 %1").arg(root.entry.keyDisplayText); onClicked: root.page.removeBinding(root.entry.id) }
            }
        }
    }
}
