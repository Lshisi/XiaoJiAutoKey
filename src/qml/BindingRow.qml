import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import XiaoJi.UI

Item {
    id: root
    required property QtObject page
    required property var entry
    implicitHeight: 40
    Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Theme.divider }
    RowLayout {
        anchors.fill: parent
        spacing: 0
        QuietCheck {
            Layout.preferredWidth: 34
            checked: root.entry.enabled
            Accessible.name: qsTr("启用 %1").arg(root.entry.keyText)
            onClicked: root.page.enableBinding(root.entry.id, checked)
        }
        QuietLabel {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            text: root.entry.keyText
            elide: Text.ElideRight
            wrapMode: Text.NoWrap
            HoverHandler { id: keyHover }
            ToolTip.visible: keyHover.hovered && (truncated || root.entry.positionHint.length > 0)
            ToolTip.text: root.entry.positionHint.length ? qsTr("%1 · %2").arg(text).arg(root.entry.positionHint) : text
        }
        QuietLabel { Layout.preferredWidth: 66; text: root.entry.modeText; font.pixelSize: 12; elide: Text.ElideRight; wrapMode: Text.NoWrap }
        QuietLabel { Layout.preferredWidth: 40; text: root.entry.priorityText; color: Theme.muted; font.pixelSize: 12 }
        QuietLabel { Layout.preferredWidth: 60; text: qsTr("%1 ms").arg(root.entry.delay); font.pixelSize: 12 }
        IconButton { glyph: "\uE70F"; hint: qsTr("编辑 %1").arg(root.entry.keyText); onClicked: root.page.editBinding(root.entry.id) }
        IconButton { glyph: "\uE74D"; hint: qsTr("删除 %1").arg(root.entry.keyText); onClicked: root.page.removeBinding(root.entry.id) }
    }
}
