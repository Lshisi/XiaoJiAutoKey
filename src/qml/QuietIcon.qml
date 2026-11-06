import QtQuick

Item {
    id: icon
    required property string name
    required property color color
    required property int pixelSize
    implicitWidth: pixelSize
    implicitHeight: pixelSize
    Accessible.ignored: true
    Image {
        anchors.centerIn: parent
        width: icon.pixelSize
        height: icon.pixelSize
        source: "image://app-icons/icons/" + icon.name + ".svg/" + icon.color.toString().substring(1)
        sourceSize: Qt.size(Math.ceil(width * Screen.devicePixelRatio), Math.ceil(height * Screen.devicePixelRatio))
        fillMode: Image.PreserveAspectFit
        smooth: true
        onStatusChanged: { if (status === Image.Error) console.error("Icon failed:", source) }
        Accessible.ignored: true
    }
}
