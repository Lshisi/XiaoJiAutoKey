import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import XiaoJi.UI

Item {
    id: root
    required property QtObject page
    readonly property var keys: page.state.pauseKeys
    readonly property real moreWidth: Math.max(Theme.iconSize, metrics.advanceWidth(qsTr("+%1").arg(keys.length)) + 8)
    readonly property int visibleCount: {
        let used = 0
        let count = 0
        for (let i = 0; i < Math.min(keys.length, 3); ++i) {
            const next = Math.min(Math.ceil(metrics.advanceWidth(keys[i])) + 2, Theme.shortcutTextMaxWidth)
                + Theme.shortcutRemoveWidth + Theme.shortcutTagGap + 2 * Theme.shortcutTagPadding
            const more = i + 1 < keys.length ? moreWidth + 4 : 0
            if (used + next + more > width) break
            used += next + 4
            ++count
        }
        return count
    }
    implicitWidth: root.keys.length === 0 ? Math.ceil(metrics.advanceWidth(qsTr("未设置"))) + 2 : moreWidth
    implicitHeight: Math.max(Theme.compactIconSize, metrics.height + 4)
    FontMetrics { id: metrics; font.family: Theme.fontFamily; font.pixelSize: Theme.bodyFontSize }
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
                enabled: !root.page.state.running
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
            hint: qsTr("更多暂停键，点击可删除")
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
                    delegate: QuietMenuItem {
                        required property string modelData
                        required property int index
                        width: moreList.width
                        text: qsTr("%1 ×").arg(modelData)
                        onTriggered: { const position = root.visibleCount + index; morePopup.close(); root.page.pauseKeyDeleteClicked(position) }
                    }
                    ScrollBar.vertical: QuietScrollBar {}
                }
                background: Rectangle { color: Theme.surface; radius: Theme.inputRadius; border.color: Theme.border }
            }
        }
        Item { Layout.fillWidth: true }
    }
}
