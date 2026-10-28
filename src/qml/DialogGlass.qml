import QtQuick
import XiaoJi.UI

Item {
    id: glass
    required property url sourceUrl
    required property vector4d sourceRegion
    Image {
        id: scene
        source: glass.sourceUrl
        sourceSize: Qt.size(Math.ceil(glass.width * Screen.devicePixelRatio / glass.sourceRegion.z), Math.ceil(glass.height * Screen.devicePixelRatio / glass.sourceRegion.w))
        visible: false
        cache: false
        Accessible.ignored: true
    }
    ShaderEffect {
        id: horizontal
        anchors.fill: parent
        property variant sourceTexture: scene
        property vector4d sampleRegion: glass.sourceRegion
        property vector2d sampleStep: Qt.vector2d(glass.sourceRegion.z * 2 / Math.max(1, glass.width), 0)
        fragmentShader: "qrc:/shaders/glass-blur.frag.qsb"
    }
    ShaderEffectSource {
        id: horizontalTexture
        sourceItem: horizontal
        smooth: true
        hideSource: true
        visible: false
        textureSize: Qt.size(Math.ceil(glass.width * Screen.devicePixelRatio / 2), Math.ceil(glass.height * Screen.devicePixelRatio / 2))
    }
    ShaderEffect {
        id: vertical
        anchors.fill: parent
        property variant sourceTexture: horizontalTexture
        property vector4d sampleRegion: Qt.vector4d(0, 0, 1, 1)
        property vector2d sampleStep: Qt.vector2d(0, 2 / Math.max(1, glass.height))
        fragmentShader: "qrc:/shaders/glass-blur.frag.qsb"
    }
    ShaderEffectSource {
        id: blurredTexture
        sourceItem: vertical
        smooth: true
        hideSource: true
        visible: false
        textureSize: horizontalTexture.textureSize
    }
    ShaderEffect {
        anchors.fill: parent
        property variant sourceTexture: blurredTexture
        property size surfaceSize: Qt.size(width, height)
        property color surfaceColor: Theme.surface
        property real darkAmount: Theme.dark ? 1 : 0
        property real cornerRadius: Theme.dialogRadius
        fragmentShader: "qrc:/shaders/glass-refract.frag.qsb"
        Accessible.ignored: true
    }
}
