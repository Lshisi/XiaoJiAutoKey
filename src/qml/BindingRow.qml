import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import XiaoJi.UI

Item {
    id: root
    required property QtObject page
    required property var entry
    implicitHeight: Theme.tableRowHeight
    HoverHandler { id: rowHover }
    MouseArea { anchors.fill: parent; onDoubleClicked: root.page.editBinding(root.entry.id) }
    Rectangle { anchors.fill: parent; color: rowHover.hovered ? Theme.hover : "transparent" }
    Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Theme.divider }
    RowLayout {
        anchors.fill: parent
        spacing: 0
        Item {
            Layout.preferredWidth: 42
            Layout.fillHeight: true
            QuietCheck {
                anchors.centerIn: parent
                checked: root.entry.enabled
                Accessible.name: qsTr("启用 %1").arg(root.entry.keyText)
                onClicked: root.page.enableBinding(root.entry.id, checked)
            }
        }
        QuietLabel {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            Layout.preferredWidth: 0
            text: root.entry.keyText
            elide: Text.ElideRight
            wrapMode: Text.NoWrap
            Accessible.description: root.entry.positionHint
        }
        QuietLabel { Layout.fillWidth: true; Layout.preferredWidth: 0; Layout.minimumWidth: 0; text: root.entry.modeText; elide: Text.ElideRight; wrapMode: Text.NoWrap }
        QuietLabel {
            Layout.fillWidth: true
            Layout.preferredWidth: 0
            Layout.minimumWidth: 0
            text: root.entry.priorityText
            horizontalAlignment: Text.AlignHCenter
            color: root.entry.priorityKind === "enabled" ? Theme.text : Theme.muted
        }
        QuietLabel { Layout.fillWidth: true; Layout.preferredWidth: 0; Layout.minimumWidth: 0; text: root.entry.delay.toString(); elide: Text.ElideRight; wrapMode: Text.NoWrap }
        Item { Layout.preferredWidth: 4 }
        IconButton { glyph: "\uE70F"; hint: qsTr("编辑 %1").arg(root.entry.keyText); onClicked: root.page.editBinding(root.entry.id) }
        IconButton { glyph: "\uE74D"; hint: qsTr("删除 %1").arg(root.entry.keyText); onClicked: root.page.removeBinding(root.entry.id) }
        Item { Layout.preferredWidth: 4 }
    }
}
