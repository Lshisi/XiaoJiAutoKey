import QtQuick
import QtQuick.Layouts
import XiaoJi.UI

ColumnLayout {
    id: root
    required property QtObject calendar
    spacing: 10
    RowLayout {
        Layout.fillWidth: true
        ActionButton {
            text: qsTr("上月")
            quiet: true
            compact: true
            enabled: root.calendar.canPreviousMonth
            onClicked: root.calendar.moveMonth(-1)
        }
        QuietHeading {
            Layout.fillWidth: true
            text: root.calendar.monthLabel
            horizontalAlignment: Text.AlignHCenter
        }
        ActionButton {
            text: qsTr("下月")
            quiet: true
            compact: true
            enabled: root.calendar.canNextMonth
            onClicked: root.calendar.moveMonth(1)
        }
    }
    GridLayout {
        Layout.fillWidth: true
        columns: 7
        columnSpacing: 4
        rowSpacing: 5
        Repeater {
            model: [qsTr("一"), qsTr("二"), qsTr("三"), qsTr("四"), qsTr("五"), qsTr("六"), qsTr("日")]
            delegate: QuietLabel {
                required property string modelData
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                Layout.preferredHeight: 24
                text: modelData
                color: Theme.muted
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                font.pixelSize: Theme.captionFontSize
            }
        }
        Repeater {
            model: root.calendar.monthCells
            delegate: ActionButton {
                id: day
                required property var modelData
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                Layout.preferredHeight: 29
                text: modelData.inMonth ? String(modelData.day) : ""
                quiet: true
                compact: true
                leftPadding: 4
                rightPadding: 4
                accented: modelData.selected && modelData.inMonth
                enabled: modelData.inMonth && modelData.inRange
                font.weight: modelData.today ? Theme.headingWeight : Theme.bodyWeight
                hint: modelData.inMonth ? modelData.date : ""
                Accessible.name: modelData.date
                Accessible.ignored: !modelData.inMonth
                onClicked: root.calendar.selectDate(modelData.date)
                Rectangle {
                    visible: day.modelData.today && day.modelData.inMonth
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: 2
                    width: 3
                    height: 3
                    radius: 1.5
                    color: Theme.accent
                    Accessible.ignored: true
                }
            }
        }
    }
    QuietLabel {
        Layout.fillWidth: true
        Layout.topMargin: 4
        text: qsTr("%1 — %2").arg(root.calendar.minimumDate.slice(5)).arg(root.calendar.maximumDate.slice(5))
        horizontalAlignment: Text.AlignHCenter
        color: Theme.muted
        font.pixelSize: Theme.captionFontSize
    }
    QuietLabel {
        Layout.fillWidth: true
        text: qsTr("可查看前后 15 天")
        horizontalAlignment: Text.AlignHCenter
        color: Theme.muted
        font.pixelSize: Theme.captionFontSize
    }
}
