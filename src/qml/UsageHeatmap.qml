import QtQuick
import XiaoJi.UI

FocusScope {
    id: chart
    required property var days
    required property var months
    required property int weeks
    required property string periodLabel
    property Item hoveredCell: null
    property int focusedIndex: -1
    readonly property int labelWidth: 26
    readonly property int labelHeight: 22
    readonly property int cellGap: 3
    readonly property int rowGap: cellGap
    readonly property real cellSize: Math.max(10, Math.min(14,
        (width - labelWidth - (weeks - 1) * cellGap) / weeks))
    readonly property real cellPitch: Math.max(cellSize + cellGap,
        (width - labelWidth - cellSize) / (weeks - 1))
    readonly property real rowPitch: cellSize + rowGap
    readonly property var tooltipCell: hoveredCell ? hoveredCell
        : activeFocus && focusedIndex >= 0 ? dayCells.itemAt(focusedIndex) : null
    implicitHeight: labelHeight + 7 * cellSize + 6 * rowGap
    activeFocusOnTab: true
    Accessible.role: Accessible.Grouping
    Accessible.name: qsTr("%1使用时长日历，方向键查看每日时长").arg(periodLabel)
    onPeriodLabelChanged: {
        focusedIndex = -1
        if (activeFocus) Qt.callLater(resetFocus)
    }
    onActiveFocusChanged: {
        if (activeFocus && focusedIndex < 0) resetFocus()
    }

    function resetFocus() {
        focusedIndex = days.findIndex(day => day.today)
        if (focusedIndex < 0) focusedIndex = days.findIndex(day => day.inRange)
    }

    function moveFocus(offset) {
        const next = focusedIndex + offset
        if (next >= 0 && next < days.length && days[next].inRange) focusedIndex = next
    }

    Keys.onLeftPressed: moveFocus(-7)
    Keys.onRightPressed: moveFocus(7)
    Keys.onUpPressed: moveFocus(-1)
    Keys.onDownPressed: moveFocus(1)

    Item {
        width: chart.width
        height: chart.implicitHeight
        Repeater {
            model: chart.months
            delegate: QuietLabel {
                required property int index
                required property var modelData
                x: chart.labelWidth + modelData.column * chart.cellPitch
                visible: index > 0 || chart.months.length < 2
                    || (chart.months[1].column - modelData.column) * chart.cellPitch >= implicitWidth + 4
                text: modelData.label
                color: Theme.muted
                font.pixelSize: Theme.captionFontSize
                wrapMode: Text.NoWrap
            }
        }
        Repeater {
            model: [qsTr("一"), qsTr("三"), qsTr("五"), qsTr("日")]
            delegate: QuietLabel {
                required property int index
                required property string modelData
                y: chart.labelHeight + index * 2 * chart.rowPitch + (chart.cellSize - height) / 2
                width: chart.labelWidth - 8
                height: Math.max(implicitHeight, chart.cellSize)
                text: modelData
                color: Theme.muted
                font.pixelSize: Theme.captionFontSize
                verticalAlignment: Text.AlignVCenter
                wrapMode: Text.NoWrap
                Accessible.name: qsTr("星期%1").arg(modelData)
            }
        }
        Repeater {
            id: dayCells
            model: chart.days.length
            delegate: Rectangle {
                id: dayCell
                required property int index
                readonly property var day: chart.days[index]
                x: chart.labelWidth + Math.floor(index / 7) * chart.cellPitch
                y: chart.labelHeight + index % 7 * chart.rowPitch
                width: chart.cellSize
                height: chart.cellSize
                radius: 2
                visible: day.inRange
                color: Theme.usageHeatmapColors[day.level]
                border.width: day.today || chart.hoveredCell === dayCell
                    || chart.activeFocus && chart.focusedIndex === index ? 1 : 0
                border.color: day.today || chart.activeFocus && chart.focusedIndex === index ? Theme.accent : Theme.muted
                Accessible.role: Accessible.StaticText
                Accessible.name: day.tooltip
                HoverHandler {
                    onHoveredChanged: {
                        if (hovered) chart.hoveredCell = dayCell
                        else if (chart.hoveredCell === dayCell) chart.hoveredCell = null
                    }
                }
                TapHandler {
                    onTapped: {
                        chart.focusedIndex = dayCell.index
                        chart.forceActiveFocus()
                    }
                }
            }
        }
    }
    QuietToolTip {
        parent: chart.tooltipCell ? chart.tooltipCell : chart
        visible: chart.tooltipCell !== null && chart.visible
        text: chart.tooltipCell ? chart.tooltipCell.day.tooltip : ""
        x: (parent.width - width) / 2
        y: -implicitHeight - 6
        timeout: -1
    }
}
