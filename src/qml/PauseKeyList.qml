pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import XiaoJi.UI

Item {
    id: root
    required property QtObject page
    readonly property var keys: page.state.pauseKeys
    readonly property real moreWidth: Math.max(Theme.iconSize, metrics.advanceWidth(qsTr("+%1").arg(keys.length)) + 8)
    readonly property var tagWidths: keys.slice(0, 3).map(key =>
        Math.min(Math.ceil(metrics.advanceWidth(key)) + 2, Theme.shortcutTextMaxWidth)
            + Theme.shortcutRemoveWidth + Theme.shortcutTagGap + 2 * Theme.shortcutTagPadding)
    readonly property int visibleCount: {
        let used = 0
        let count = 0
        for (let i = 0; i < tagWidths.length; ++i) {
            const next = tagWidths[i]
            const more = i + 1 < keys.length ? moreWidth + 4 : 0
            if (used + next + more > width) break
            used += next + 4
            ++count
        }
        return count
    }
    implicitWidth: {
        if (root.keys.length === 0) return Math.ceil(metrics.advanceWidth(qsTr("未设置"))) + 2
        const tags = root.tagWidths.reduce((total, tagWidth) => total + tagWidth, 0)
        return tags + (root.tagWidths.length - 1) * 4
            + (root.keys.length > root.tagWidths.length ? root.moreWidth + 4 : 0)
    }
    implicitHeight: Math.max(Theme.compactIconSize, metrics.height + 4)
    FontMetrics { id: metrics; font.family: Theme.fontFamily; font.pixelSize: Theme.bodyFontSize; font.weight: Theme.bodyWeight }
    RowLayout {
        anchors.fill: parent
        spacing: 4
        ShortcutTag { visible: root.keys.length === 0; text: qsTr("未设置"); removable: false; enabled: !root.page.state.running }
        Repeater {
            model: root.visibleCount
            delegate: ShortcutTag {
                id: tag
                required property int index
                Layout.minimumWidth: implicitWidth
                Layout.preferredWidth: implicitWidth
                Layout.maximumWidth: implicitWidth
                Layout.preferredHeight: implicitHeight
                text: root.keys[tag.index]
                removeHint: qsTr("删除暂停键 %1").arg(root.keys[tag.index])
                editable: true
                editHint: qsTr("编辑暂停键 %1").arg(root.keys[tag.index])
                enabled: !root.page.state.running
                onEditRequested: root.page.pauseKeyEditClicked(tag.index)
                onRemoveRequested: root.page.pauseKeyDeleteClicked(tag.index)
            }
        }
        ActionButton {
            id: moreButton
            visible: root.keys.length > root.visibleCount
            Layout.preferredWidth: root.moreWidth
            implicitHeight: Math.max(Theme.compactIconSize, contentItem.implicitHeight + topPadding + bottomPadding)
            leftPadding: 4
            rightPadding: 4
            topPadding: 2
            bottomPadding: 2
            text: qsTr("+%1").arg(root.keys.length - root.visibleCount)
            hint: qsTr("更多暂停键，点击可编辑或删除")
            enabled: !root.page.state.running
            onClicked: morePopup.open()
            Popup {
                id: morePopup
                y: -height - 4
                width: 180
                height: Math.min(moreList.contentHeight + 8, 232)
                padding: 4
                margins: 8
                contentItem: ListView {
                    id: moreList
                    clip: true
                    model: root.keys.slice(root.visibleCount)
                    delegate: RowLayout {
                        id: pauseEntry
                        required property string modelData
                        required property int index
                        width: moreList.width
                        spacing: 4
                        QuietMenuItem {
                            Layout.fillWidth: true
                            text: qsTr("编辑 %1").arg(pauseEntry.modelData)
                            onTriggered: { const position = root.visibleCount + pauseEntry.index; morePopup.close(); root.page.pauseKeyEditClicked(position) }
                        }
                        IconButton {
                            iconName: "close"
                            hint: qsTr("删除暂停键 %1").arg(pauseEntry.modelData)
                            onClicked: { const position = root.visibleCount + pauseEntry.index; morePopup.close(); root.page.pauseKeyDeleteClicked(position) }
                        }
                    }
                    ScrollBar.vertical: QuietScrollBar {}
                }
                background: Rectangle { color: Theme.surface; radius: Theme.inputRadius; border.color: Theme.border }
            }
        }
        Item { Layout.fillWidth: true }
    }
}
