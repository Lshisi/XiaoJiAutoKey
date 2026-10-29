import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import XiaoJi.UI

Item {
    id: root
    required property QtObject page
    readonly property var keys: page.state.pauseKeys
    readonly property int visibleCount: {
        let used = 0
        let count = 0
        for (let i = 0; i < Math.min(keys.length, 3); ++i) {
            const next = Math.min(metrics.advanceWidth(keys[i]), 72) + Theme.iconSize + 12
            const more = i + 1 < keys.length ? 46 : 0
            if (used + next + more > width) break
            used += next + 4
            ++count
        }
        return count
    }
    implicitHeight: Theme.iconSize
    FontMetrics { id: metrics; font.family: Theme.fontFamily; font.pixelSize: 12 }
    RowLayout {
        anchors.fill: parent
        spacing: 4
        QuietLabel { visible: root.keys.length === 0; text: qsTr("未设置"); color: Theme.muted }
        Repeater {
            model: root.visibleCount
            delegate: Rectangle {
                id: tag
                required property int index
                Layout.preferredWidth: Math.min(metrics.advanceWidth(root.keys[index]), 72) + Theme.iconSize + 12
                Layout.preferredHeight: Theme.iconSize
                radius: 4
                color: Theme.hover
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 6
                    spacing: 2
                    QuietLabel { Layout.fillWidth: true; Layout.minimumWidth: 0; text: root.keys[tag.index]; elide: Text.ElideRight; wrapMode: Text.NoWrap }
                    IconButton { glyph: "\uE711"; hint: qsTr("删除暂停键 %1").arg(root.keys[tag.index]); enabled: !root.page.state.running; onClicked: root.page.pauseKeyDeleteClicked(tag.index) }
                }
            }
        }
        ActionButton {
            id: moreButton
            visible: root.keys.length > root.visibleCount
            Layout.preferredWidth: 42
            leftPadding: 4
            rightPadding: 4
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
