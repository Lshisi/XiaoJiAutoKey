import QtQuick
import QtQuick.Layouts
import QtQuick.Shapes
import XiaoJi.UI

ColumnLayout {
    id: trend
    required property var months
    required property var ticks
    required property var years
    required property int yearIndex
    readonly property var currentMonth: months.find(month => month.current)
    property var hoveredMonth: null
    signal yearSelected(int year)
    spacing: 6
    RowLayout {
        Layout.fillWidth: true
        QuietHeading { Layout.fillWidth: true; text: qsTr("全年月度趋势") }
        QuietLabel {
            visible: trend.currentMonth !== undefined
            text: trend.currentMonth ? qsTr("本月累计 %1").arg(trend.currentMonth.duration) : ""
            color: Theme.muted
            font.pixelSize: Theme.captionFontSize
        }
        QuietCombo {
            Layout.preferredWidth: 100
            model: trend.years
            currentIndex: trend.yearIndex
            onActivated: trend.yearSelected(Number(trend.years[currentIndex]))
            Accessible.name: qsTr("月度统计年份")
        }
    }
    Item {
        id: plot
        Layout.fillWidth: true
        readonly property int axisWidth: 44
        readonly property int topPadding: 7
        readonly property int plotHeight: 52
        readonly property real lineWidth: width - axisWidth
        readonly property real columnWidth: lineWidth / trend.months.length
        readonly property int currentIndex: trend.months.findIndex(month => month.current)
        readonly property var completedPoints: trend.months.map((month, index) =>
            !month.future && !month.current
                ? Qt.point((index + 0.5) * columnWidth, (1 - month.ratio) * plotHeight) : null)
            .filter(point => point !== null)
        readonly property var currentSegment: currentIndex > 0 ? [
            Qt.point((currentIndex - 0.5) * columnWidth, (1 - trend.months[currentIndex - 1].ratio) * plotHeight),
            Qt.point((currentIndex + 0.5) * columnWidth, (1 - trend.months[currentIndex].ratio) * plotHeight)
        ] : []
        implicitHeight: topPadding + plotHeight + 6 + captionMetrics.height
        FontMetrics {
            id: captionMetrics
            font.family: Theme.fontFamily
            font.pixelSize: Theme.captionFontSize
        }
        Repeater {
            model: trend.ticks
            delegate: Item {
                required property var modelData
                y: plot.topPadding + (1 - modelData.ratio) * plot.plotHeight
                width: plot.width
                height: 1
                Rectangle { x: plot.axisWidth; width: plot.lineWidth; height: 1; color: Theme.divider; Accessible.ignored: true }
                QuietLabel {
                    y: -implicitHeight / 2
                    width: plot.axisWidth - 8
                    text: modelData.label
                    color: Theme.muted
                    font.pixelSize: Theme.captionFontSize
                    wrapMode: Text.NoWrap
                }
            }
        }
        Shape {
            preferredRendererType: Shape.CurveRenderer
            x: plot.axisWidth
            y: plot.topPadding
            width: plot.lineWidth
            height: plot.plotHeight
            visible: plot.completedPoints.length > 1
            Accessible.ignored: true
            ShapePath {
                strokeColor: Theme.accent
                strokeWidth: 1.5
                fillColor: "transparent"
                capStyle: ShapePath.RoundCap
                joinStyle: ShapePath.RoundJoin
                startX: plot.completedPoints.length ? plot.completedPoints[0].x : 0
                startY: plot.completedPoints.length ? plot.completedPoints[0].y : 0
                PathPolyline { path: plot.completedPoints }
            }
        }
        Shape {
            preferredRendererType: Shape.CurveRenderer
            x: plot.axisWidth
            y: plot.topPadding
            width: plot.lineWidth
            height: plot.plotHeight
            visible: plot.currentSegment.length > 1
            Accessible.ignored: true
            ShapePath {
                strokeColor: Theme.accent
                strokeWidth: 1.5
                strokeStyle: ShapePath.DashLine
                dashPattern: [3, 3]
                fillColor: "transparent"
                startX: plot.currentSegment.length ? plot.currentSegment[0].x : 0
                startY: plot.currentSegment.length ? plot.currentSegment[0].y : 0
                PathPolyline { path: plot.currentSegment }
            }
        }
        Repeater {
            model: trend.months.length
            delegate: Item {
                id: monthColumn
                required property int index
                readonly property var month: trend.months[index]
                x: plot.axisWidth + index * plot.columnWidth
                width: plot.columnWidth
                height: plot.height
                Accessible.role: Accessible.StaticText
                Accessible.name: month.tooltip
                Rectangle {
                    visible: !monthColumn.month.future && (monthColumn.month.current || trend.hoveredMonth === monthColumn)
                    width: 6
                    height: 6
                    x: (monthColumn.width - width) / 2
                    y: plot.topPadding + (1 - monthColumn.month.ratio) * plot.plotHeight - height / 2
                    radius: 3
                    color: monthColumn.month.current ? Theme.surface : Theme.accent
                    border.width: 1.5
                    border.color: Theme.accent
                    Accessible.ignored: true
                }
                QuietLabel {
                    y: plot.topPadding + plot.plotHeight + 6
                    width: monthColumn.width
                    text: monthColumn.month.label
                    color: monthColumn.month.current ? Theme.accent : Theme.muted
                    font.pixelSize: Theme.captionFontSize
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.NoWrap
                }
                HoverHandler {
                    onHoveredChanged: {
                        if (hovered) trend.hoveredMonth = monthColumn
                        else if (trend.hoveredMonth === monthColumn) trend.hoveredMonth = null
                    }
                }
            }
        }
    }
    QuietToolTip {
        parent: trend.hoveredMonth ? trend.hoveredMonth : trend
        visible: trend.hoveredMonth !== null && trend.visible
        text: trend.hoveredMonth ? trend.hoveredMonth.month.tooltip : ""
        x: (parent.width - width) / 2
        y: -implicitHeight - 6
    }
}
