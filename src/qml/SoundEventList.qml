pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import XiaoJi.UI

ColumnLayout {
    id: list
    required property var events
    required property string selectedEvent
    signal eventSelected(string eventId)
    spacing: 2

    QuietLabel {
        Layout.fillWidth: true
        Layout.bottomMargin: 6
        text: qsTr("提示事件")
        color: Theme.muted
    }
    Repeater {
        model: list.events
        delegate: ColumnLayout {
            id: entry
            required property int index
            required property var modelData
            readonly property bool selected: modelData.id === list.selectedEvent
            Layout.fillWidth: true
            spacing: 2

            QuietLabel {
                Layout.fillWidth: true
                Layout.topMargin: Theme.cardGap
                visible: entry.modelData.group === 2
                    && (entry.index === 0 || list.events[entry.index - 1].group !== 2)
                text: qsTr("录制")
                color: Theme.muted
            }
            ActionButton {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                text: entry.modelData.enabled ? entry.modelData.title
                    : qsTr("%1 · 关闭").arg(entry.modelData.title)
                textAlignment: Text.AlignLeft
                quiet: true
                accented: entry.selected
                font.weight: entry.selected ? Theme.headingWeight : Theme.bodyWeight
                hint: entry.modelData.description
                Accessible.description: entry.selected
                    ? qsTr("当前选中。%1").arg(entry.modelData.description) : entry.modelData.description
                onClicked: list.eventSelected(entry.modelData.id)
            }
        }
    }
}
