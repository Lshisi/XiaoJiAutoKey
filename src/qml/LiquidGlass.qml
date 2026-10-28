import QtQuick

ShaderEffect {
    id: glass
    property bool dark: false
    required property color surfaceColor
    property real materialOpacity: 0.7
    property real cornerRadius: 18
    property real shadowInset: 8
    property point lightPoint: Qt.point(0.25, 0.08)
    readonly property size surfaceSize: Qt.size(width, height)
    readonly property real darkAmount: dark ? 1 : 0
    fragmentShader: "qrc:/shaders/liquid-glass.frag.qsb"
    Accessible.ignored: true
}
