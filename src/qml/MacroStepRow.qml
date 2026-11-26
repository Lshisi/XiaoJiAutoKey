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
    readonly property bool selectionActive: root.page.state.selectedStepIds.length > 0
    readonly property bool selected: root.page.state.selectedStepIds.indexOf(root.entry.id) >= 0
    implicitHeight: Theme.tableRowHeight
    HoverHandler { id: rowHover }
    z: dragHandler.active ? 2 : 0
    transform: Translate { y: dragHandler.active ? root.dragOffset : 0 }
    Rectangle {
        anchors.fill: parent
        color: dragHandler.active ? Theme.accentSoft : root.selected ? Theme.accentSoft
            : root.enabled && rowHover.hovered ? Theme.accentSubtle : "transparent"
    }
    MouseArea {
        anchors.fill: parent
        onDoubleClicked: mouse => {
            if (!root.selectionActive) {
                CrispyEffects.burst(root, Qt.point(mouse.x, mouse.y))
                root.page.editStepRequested(root.entry.id)
            }
        }
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
                Accessible.name: qsTr("选择步骤 %1").arg(root.rowIndex + 1)
                onClicked: root.page.toggleStepSelection(root.entry.id)
            }
        }
        IconButton {
            compact: true
            Layout.minimumWidth: Theme.compactIconSize
            Layout.maximumWidth: Theme.compactIconSize
            iconName: "drag-handle"
            hint: qsTr("拖动排序；Alt+↑/↓ 调整步骤位置")
            Accessible.name: qsTr("步骤 %1 的排序手柄").arg(root.rowIndex + 1)
            Keys.onPressed: event => {
                if (!(event.modifiers & Qt.AltModifier)) return
                const offset = event.key === Qt.Key_Up ? -1 : event.key === Qt.Key_Down ? 1 : 0
                const targetIndex = root.rowIndex + offset
                if (offset && targetIndex >= 0 && targetIndex < root.rowCount) {
                    root.page.stepMoveRequested(root.entry.id, targetIndex)
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
                        if (targetIndex !== root.rowIndex) root.page.stepMoveRequested(root.entry.id, targetIndex)
                    }
                }
            }
        }
        KeyCell {
            Layout.minimumWidth: root.columns.keyWidth
            Layout.maximumWidth: root.columns.keyWidth
            text: root.entry.keyText
            description: root.entry.positionHint
        }
        NameCell { Layout.minimumWidth: root.columns.nameWidth; Layout.maximumWidth: root.columns.nameWidth; name: root.entry.name }
        QuietLabel { Layout.minimumWidth: root.columns.delayWidth; Layout.maximumWidth: root.columns.delayWidth; text: root.entry.delay.toString(); horizontalAlignment: Text.AlignHCenter; elide: Text.ElideRight; wrapMode: Text.NoWrap }
        WorkspaceEnabledCell {
            Layout.minimumWidth: root.columns.statusWidth
            Layout.maximumWidth: root.columns.statusWidth
            Layout.fillHeight: true
            page: root.page
            entry: root.entry
            macroMode: true
        }
        Item {
            Layout.minimumWidth: root.columns.actionWidth
            Layout.maximumWidth: root.columns.actionWidth
            Layout.fillHeight: true
            Row {
                anchors.centerIn: parent
                visible: !root.selectionActive
                spacing: 0
                MacroStepMenu {
                    page: root.page
                    stepId: root.entry.id
                    actionsEnabled: root.enabled
                }
                IconButton {
                    iconName: "edit"
                    hint: qsTr("编辑步骤")
                    onClicked: root.page.editStepRequested(root.entry.id)
                }
                IconButton {
                    iconName: "delete"
                    hint: qsTr("删除步骤")
                    onClicked: root.page.deleteStepRequested(root.entry.id)
                }
            }
        }
    }
}
