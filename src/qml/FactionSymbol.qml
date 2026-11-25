import QtQuick

Image {
    id: symbol
    required property url iconSource
    required property color tintColor
    property bool preserveColors: false
    source: "image://app-icons/" + iconSource.toString().substring("qrc:/assets/".length)
        + "/" + (preserveColors ? "original" : tintColor.toString().substring(1))
    sourceSize: Qt.size(Math.ceil(width * Screen.devicePixelRatio), Math.ceil(height * Screen.devicePixelRatio))
    fillMode: Image.PreserveAspectFit
    smooth: true
    onStatusChanged: { if (status === Image.Error) console.error("Faction symbol failed:", source) }
    Accessible.ignored: true
}
