import QtQuick
import QtQuick.Shapes
import XiaoJi.UI

Item {
    id: paper
    readonly property real tearHeight: 9
    readonly property real tearOverlap: 1
    readonly property var tearOutline: {
        const count = Math.max(1, Math.round(paper.width / 20))
        const step = paper.width / count
        const points = [Qt.point(0, 0), Qt.point(paper.width, 0), Qt.point(paper.width, paper.tearHeight + paper.tearOverlap)]
        for (let i = count - 1; i >= 0; --i) {
            points.push(Qt.point((i + 0.5) * step, paper.tearOverlap))
            points.push(Qt.point(i * step, paper.tearHeight + paper.tearOverlap))
        }
        points.push(Qt.point(0, 0))
        return points
    }
    readonly property string stripeOutline: {
        let path = ""
        for (let x = -13; x < paper.width + 26; x += 26)
            path += "M" + x + " 0h13l-2 5h-13z "
        return path
    }
    Accessible.ignored: true
    Rectangle {
        anchors.fill: parent
        anchors.bottomMargin: paper.tearHeight
        color: Theme.crispyPaper
    }
    Shape {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: paper.tearHeight + paper.tearOverlap
        preferredRendererType: Shape.CurveRenderer
        ShapePath {
            fillColor: Theme.crispyPaper
            strokeColor: "transparent"
            strokeWidth: 0
            PathPolyline { path: paper.tearOutline }
        }
    }
    Rectangle {
        width: paper.width
        height: 5
        color: Theme.crispyCream
        clip: true
        Shape {
            anchors.fill: parent
            preferredRendererType: Shape.CurveRenderer
            ShapePath {
                fillColor: Theme.crispyBrand
                strokeColor: "transparent"
                strokeWidth: 0
                PathSvg { path: paper.stripeOutline }
            }
        }
    }
}
