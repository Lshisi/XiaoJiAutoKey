import QtQuick
import QtQuick.Layouts
import XiaoJi.UI

Item {
    id: root
    readonly property QtObject page: usagePageModel

    ColumnLayout {
        anchors.fill: parent
        QuietCard {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 0
            Layout.leftMargin: Theme.pageMargin
            Layout.rightMargin: Theme.pageMargin
            Layout.topMargin: Theme.cardGap
            Layout.bottomMargin: Theme.cardGap
            implicitHeight: body.implicitHeight + topPadding + bottomPadding
            ColumnLayout {
                id: body
                anchors.fill: parent
                spacing: 10
                RowLayout {
                    Layout.fillWidth: true
                    QuietHeading { Layout.fillWidth: true; text: qsTr("使用统计"); pageTitle: true; Layout.alignment: Qt.AlignTop }
                    QuietLabel { visible: root.page.state.running; text: qsTr("正在累计"); color: Theme.accent }
                }
                RowLayout {
                    Layout.fillWidth: true
                    Layout.topMargin: 4
                    Layout.bottomMargin: 4
                    spacing: Theme.cardGap
                    uniformCellSizes: true
                    Repeater {
                        model: [
                            { title: qsTr("今日使用"), value: root.page.state.todayDuration },
                            { title: qsTr("累计使用"), value: root.page.state.totalDuration }
                        ]
                        delegate: ColumnLayout {
                            id: summary
                            required property var modelData
                            Layout.fillWidth: true
                            Layout.minimumWidth: 0
                            spacing: 4
                            QuietLabel { Layout.fillWidth: true; text: summary.modelData.title; color: Theme.muted }
                            QuietHeading { Layout.fillWidth: true; text: summary.modelData.value; pageTitle: true }
                        }
                    }
                }
                Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Theme.divider; Accessible.ignored: true }
                RowLayout {
                    Layout.fillWidth: true
                    QuietHeading { Layout.fillWidth: true; text: qsTr("最近%1天").arg(root.page.state.rangeDays) }
                    QuietLabel {
                        text: root.page.state.periodLabel
                        color: Theme.muted
                        font.pixelSize: Theme.captionFontSize
                    }
                }
                UsageHeatmap {
                    Layout.fillWidth: true
                    days: root.page.state.days
                    months: root.page.state.months
                    weeks: root.page.state.weeks
                    periodLabel: root.page.state.periodLabel
                }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 4
                    QuietLabel {
                        Layout.fillWidth: true
                        text: root.page.state.hasUsage
                            ? qsTr("使用%1天 · %2").arg(root.page.state.activeDays).arg(root.page.state.periodDuration)
                            : qsTr("最近%1天暂无使用记录。").arg(root.page.state.rangeDays)
                        color: Theme.muted
                        font.pixelSize: Theme.captionFontSize
                    }
                    QuietLabel { text: qsTr("少"); color: Theme.muted; font.pixelSize: Theme.captionFontSize }
                    Repeater {
                        model: Theme.usageHeatmapColors
                        delegate: Rectangle {
                            required property color modelData
                            implicitWidth: 10
                            implicitHeight: 10
                            radius: 2
                            color: modelData
                            Accessible.ignored: true
                        }
                    }
                    QuietLabel { text: qsTr("多"); color: Theme.muted; font.pixelSize: Theme.captionFontSize }
                }
                Rectangle {
                    Layout.fillWidth: true
                    Layout.topMargin: 4
                    Layout.bottomMargin: 4
                    implicitHeight: 1
                    color: Theme.divider
                    Accessible.ignored: true
                }
                UsageMonthlyTrend {
                    Layout.fillWidth: true
                    Layout.minimumHeight: implicitHeight
                    Layout.maximumHeight: implicitHeight
                    months: root.page.state.monthlyUsage
                    ticks: root.page.state.monthlyTicks
                    years: root.page.state.years
                    yearIndex: root.page.state.yearIndex
                    onYearSelected: year => root.page.selectYear(year)
                }
                Item { Layout.fillHeight: true; Layout.minimumHeight: 0 }
            }
        }
    }
}
