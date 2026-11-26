pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Shapes
import XiaoJi.UI

Rectangle {
    id: stripes
    property bool diagonal: false
    property color paperColor: Theme.dark ? Theme.sidebarBackground : Theme.crispyCream
    readonly property string diagonalOutline: {
        if (!stripes.diagonal || stripes.width <= 0 || stripes.height <= 0) return ""
        let outline = ""
        for (let x = 0; x < stripes.width + stripes.height; x += 26)
            outline += "M" + x + " 0h13l-" + stripes.height + " " + stripes.height + "h-13z "
        return outline
    }
    implicitWidth: 30
    implicitHeight: 5
    color: paperColor
    clip: diagonal
    Accessible.ignored: true
    Repeater {
        model: stripes.diagonal ? 0 : 3
        Rectangle {
            required property int index
            x: (1 + index * 2) * width
            width: stripes.width / 7
            height: stripes.height
            color: Theme.crispyBrand
        }
    }
    Loader {
        anchors.fill: parent
        active: stripes.diagonal
        sourceComponent: Shape {
            preferredRendererType: Shape.CurveRenderer
            ShapePath {
                fillColor: Theme.crispyBrand
                strokeColor: "transparent"
                strokeWidth: 0
                PathSvg { path: stripes.diagonalOutline }
            }
        }
    }
}
