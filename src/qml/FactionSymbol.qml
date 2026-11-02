import QtQuick

Image {
    id: symbol
    required property url iconSource
    required property color tintColor
    source: "image://app-icons/" + iconSource.toString().substring("qrc:/assets/".length)
        + "/" + tintColor.toString().substring(1)
    sourceSize: Qt.size(Math.ceil(width * Screen.devicePixelRatio), Math.ceil(height * Screen.devicePixelRatio))
    fillMode: Image.PreserveAspectFit
    Accessible.ignored: true
}
