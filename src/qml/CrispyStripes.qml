pragma ComponentBehavior: Bound
import QtQuick
import XiaoJi.UI

Rectangle {
    id: stripes
    implicitWidth: 30
    implicitHeight: 5
    color: Theme.crispyCream
    Accessible.ignored: true
    Repeater {
        model: 3
        Rectangle {
            required property int index
            x: (1 + index * 2) * width
            width: stripes.width / 7
            height: stripes.height
            color: Theme.crispyBrand
        }
    }
}
