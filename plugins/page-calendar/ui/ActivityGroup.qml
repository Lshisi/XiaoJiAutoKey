import QtQuick
import QtQuick.Layouts
import XiaoJi.UI

ColumnLayout {
    id: root
    required property string title
    required property var entries
    visible: entries.length > 0
    spacing: 8
    QuietHeading {
        visible: root.title.length > 0
        Layout.fillWidth: true
        text: root.title
    }
    Repeater {
        model: root.entries
        delegate: RowLayout {
            id: activity
            required property var modelData
            Layout.fillWidth: true
            Layout.minimumHeight: 24
            spacing: 10
            QuietLabel {
                Layout.preferredWidth: 64
                Layout.alignment: Qt.AlignTop
                text: activity.modelData.title
                color: Theme.muted
            }
            QuietLabel {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                Layout.alignment: Qt.AlignTop
                text: activity.modelData.value
            }
        }
    }
}
