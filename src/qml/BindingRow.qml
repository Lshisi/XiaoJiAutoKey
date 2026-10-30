import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import XiaoJi.UI

Item {
    id: root
    required property QtObject page
    required property var entry
    required property QtObject columns
    implicitHeight: Theme.tableRowHeight
    HoverHandler { id: rowHover }
    MouseArea { anchors.fill: parent; onDoubleClicked: root.page.editBinding(root.entry.id) }
    Rectangle { anchors.fill: parent; color: root.enabled && rowHover.hovered ? Theme.accentSubtle : "transparent" }
    Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Theme.divider }
    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: root.columns.contentLeftPadding
        anchors.rightMargin: root.columns.contentRightPadding
        spacing: 0
        Item {
            Layout.preferredWidth: root.columns.leadingWidth
            Layout.fillHeight: true
            QuietCheck {
                anchors.centerIn: parent
                spacing: 0
                checked: root.entry.enabled
                Accessible.name: qsTr("启用 %1").arg(root.entry.keyText)
                onClicked: root.page.enableBinding(root.entry.id, checked)
            }
        }
        QuietLabel {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            Layout.preferredWidth: root.columns.fieldWidth
            text: root.entry.keyText
            horizontalAlignment: Text.AlignHCenter
            elide: Text.ElideRight
            wrapMode: Text.NoWrap
            Accessible.description: root.entry.positionHint
        }
        QuietLabel { Layout.fillWidth: true; Layout.preferredWidth: root.columns.fieldWidth; Layout.minimumWidth: 0; text: root.entry.modeText; horizontalAlignment: Text.AlignHCenter; elide: Text.ElideRight; wrapMode: Text.NoWrap }
        QuietLabel {
            Layout.fillWidth: true
            Layout.preferredWidth: root.columns.fieldWidth
            Layout.minimumWidth: 0
            text: root.entry.priorityText
            horizontalAlignment: Text.AlignHCenter
            color: root.entry.priorityKind === "enabled" ? Theme.text : Theme.muted
        }
        QuietLabel { Layout.fillWidth: true; Layout.preferredWidth: root.columns.fieldWidth; Layout.minimumWidth: 0; text: root.entry.delay.toString(); horizontalAlignment: Text.AlignHCenter; elide: Text.ElideRight; wrapMode: Text.NoWrap }
        Item { Layout.preferredWidth: root.columns.actionPadding }
        IconButton { glyph: "\uE70F"; hint: qsTr("编辑 %1").arg(root.entry.keyText); onClicked: root.page.editBinding(root.entry.id) }
        IconButton { glyph: "\uE74D"; hint: qsTr("删除 %1").arg(root.entry.keyText); onClicked: root.page.removeBinding(root.entry.id) }
        Item { Layout.preferredWidth: root.columns.actionPadding }
    }
}
