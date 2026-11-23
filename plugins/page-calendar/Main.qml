import QtQuick
import QtQuick.Layouts
import XiaoJi.UI
import "ui" as Calendar

QuietCard {
    id: root
    required property QtObject pluginApi
    property bool previewMode: false
    property var previewPayload: null

    Calendar.CalendarModel {
        id: calendarModel
        active: root.visible
        previewMode: root.previewMode
        previewPayload: root.previewPayload
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: Theme.cardGap
        RowLayout {
            Layout.fillWidth: true
            QuietHeading {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignTop
                text: qsTr("活动日历")
                pageTitle: true
            }
            QuietLabel {
                visible: calendarModel.loading
                text: qsTr("更新中…")
                color: Theme.muted
                font.pixelSize: Theme.captionFontSize
            }
            ActionButton {
                text: qsTr("今天")
                quiet: true
                compact: true
                onClicked: calendarModel.selectDate(calendarModel.today)
            }
            IconButton {
                iconName: "refresh"
                hint: qsTr("刷新活动安排")
                enabled: !calendarModel.loading
                onClicked: calendarModel.refresh()
            }
        }
        QuietScrollView {
            id: scroll
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 0
            showScrollBar: false
            contentWidth: availableWidth
            contentHeight: workspace.implicitHeight
            GridLayout {
                id: workspace
                width: scroll.availableWidth
                columns: width >= 540 ? 2 : 1
                columnSpacing: 24
                rowSpacing: 20
                Calendar.MonthNavigator {
                    Layout.preferredWidth: workspace.columns === 2 ? 224 : -1
                    Layout.fillWidth: workspace.columns === 1
                    Layout.alignment: Qt.AlignTop
                    calendar: calendarModel
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    Layout.alignment: Qt.AlignTop
                    spacing: Theme.cardGap
                    RowLayout {
                        Layout.fillWidth: true
                        QuietHeading { Layout.fillWidth: true; text: calendarModel.selectedLabel }
                        QuietLabel {
                            visible: calendarModel.selectedDate === calendarModel.today
                            text: qsTr("今天")
                            color: Theme.accent
                            font.pixelSize: Theme.captionFontSize
                        }
                    }
                    Calendar.ActivityGroup {
                        Layout.fillWidth: true
                        title: ""
                        entries: calendarModel.dailyActivities
                    }
                    Rectangle {
                        visible: calendarModel.weeklyActivities.length > 0 && calendarModel.dailyActivities.length > 0
                        Layout.fillWidth: true
                        implicitHeight: 1
                        color: Theme.divider
                        Accessible.ignored: true
                    }
                    Calendar.ActivityGroup {
                        Layout.fillWidth: true
                        title: qsTr("当周活动")
                        entries: calendarModel.weeklyActivities
                    }
                }
            }
        }
        RowLayout {
            Layout.fillWidth: true
            QuietLabel {
                Layout.fillWidth: true
                text: qsTr("北京时间 7 点换日")
                color: Theme.muted
                font.pixelSize: Theme.captionFontSize
            }
            QuietLabel {
                text: qsTr("数据来自 JX3API")
                color: Theme.muted
                font.pixelSize: Theme.captionFontSize
            }
        }
    }
}
