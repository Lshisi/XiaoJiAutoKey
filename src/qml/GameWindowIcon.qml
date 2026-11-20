import QtQuick
import XiaoJi.UI

Item {
    id: root
    property url source
    implicitWidth: 24
    implicitHeight: 24
    Accessible.ignored: true

    Image {
        id: image
        anchors.fill: parent
        source: root.source
        sourceSize: Qt.size(Math.ceil(width * Screen.devicePixelRatio), Math.ceil(height * Screen.devicePixelRatio))
        fillMode: Image.PreserveAspectFit
        asynchronous: true
        visible: status === Image.Ready
        Accessible.ignored: true
    }
    QuietIcon {
        anchors.centerIn: parent
        name: "monitor"
        color: Theme.muted
        pixelSize: 22
        visible: image.status !== Image.Ready
    }
}
