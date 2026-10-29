import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import XiaoJi.UI

Item {
    id: root
    required property QtObject page
    required property var entry
    required property QtObject columns
    required property int rowIndex
    required property int rowCount
    property real dragOffset: 0
    implicitHeight: Theme.tableRowHeight
    HoverHandler { id: rowHover }
    z: dragHandler.active ? 2 : 0
    transform: Translate { y: dragHandler.active ? root.dragOffset : 0 }
    Rectangle { anchors.fill: parent; color: dragHandler.active ? Theme.accentSoft : rowHover.hovered ? Theme.hover : "transparent" }
    MouseArea { anchors.fill: parent; onDoubleClicked: root.page.editStepRequested(root.rowIndex) }
    Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Theme.divider }
    RowLayout {
        anchors.fill: parent
        spacing: 0
        ActionButton {
            Layout.preferredWidth: root.columns.leadingWidth
            text: "≡"
            quiet: true
            leftPadding: 4
            rightPadding: 4
            hint: qsTr("拖动排序；Alt+↑/↓ 调整步骤位置")
            Accessible.name: qsTr("步骤 %1 的排序手柄").arg(root.rowIndex + 1)
            Keys.onPressed: event => {
                if (!(event.modifiers & Qt.AltModifier)) return
                const offset = event.key === Qt.Key_Up ? -1 : event.key === Qt.Key_Down ? 1 : 0
                const targetIndex = root.rowIndex + offset
                if (offset && targetIndex >= 0 && targetIndex < root.rowCount) {
                    root.page.stepMoveRequested(root.rowIndex, targetIndex)
                    event.accepted = true
                }
            }
            DragHandler {
                id: dragHandler
                target: null
                onActiveTranslationChanged: {
                    if (active) root.dragOffset = activeTranslation.y
                }
                onActiveChanged: {
                    if (active) {
                        root.dragOffset = 0
                    } else {
                        const targetIndex = Math.max(0, Math.min(root.rowCount - 1, root.rowIndex + Math.round(root.dragOffset / root.height)))
                        root.dragOffset = 0
                        if (targetIndex !== root.rowIndex) root.page.stepMoveRequested(root.rowIndex, targetIndex)
                    }
                }
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
        QuietLabel { Layout.fillWidth: true; Layout.preferredWidth: root.columns.fieldWidth; Layout.minimumWidth: 0; text: root.entry.delay.toString(); horizontalAlignment: Text.AlignHCenter; elide: Text.ElideRight; wrapMode: Text.NoWrap }
        Item { Layout.preferredWidth: root.columns.actionPadding }
        IconButton { glyph: "\uE70F"; hint: qsTr("编辑步骤"); onClicked: root.page.editStepRequested(root.rowIndex) }
        IconButton { glyph: "\uE74D"; hint: qsTr("删除步骤"); onClicked: root.page.deleteStepRequested(root.rowIndex) }
        Item { Layout.preferredWidth: root.columns.actionPadding }
    }
}
