import QtQuick

Item {
    id: glass
    required property url backdrop
    required property bool hasBackdrop
    required property color surfaceColor
    required property real materialOpacity
    required property bool dark
    readonly property int sampleMargin: 12
    readonly property real blurRadius: 1.5
    readonly property real blurDownscale: 0.5
    Accessible.ignored: true
    Image {
        id: scene
        width: glass.width + 2 * glass.sampleMargin
        height: glass.height + 2 * glass.sampleMargin
        source: glass.backdrop
        sourceSize: Qt.size(Math.ceil(width * Screen.devicePixelRatio), Math.ceil(height * Screen.devicePixelRatio))
        cache: false
        visible: false
    }
    ShaderEffect {
        id: horizontal
        visible: false
        width: scene.width
        height: scene.height
        property variant sourceTexture: scene
        property vector2d sampleStep: Qt.vector2d(glass.blurRadius / width, 0)
        fragmentShader: "qrc:/shaders/glass-blur.frag.qsb"
    }
    ShaderEffectSource {
        id: horizontalTexture
        sourceItem: horizontal
        smooth: true
        hideSource: true
        visible: false
        textureSize: Qt.size(Math.max(1, Math.ceil(scene.width * Screen.devicePixelRatio * glass.blurDownscale)),
            Math.max(1, Math.ceil(scene.height * Screen.devicePixelRatio * glass.blurDownscale)))
    }
    ShaderEffect {
        id: vertical
        visible: false
        width: scene.width
        height: scene.height
        property variant sourceTexture: horizontalTexture
        property vector2d sampleStep: Qt.vector2d(0, glass.blurRadius / height)
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
        property vector4d sampleRegion: Qt.vector4d(glass.sampleMargin / scene.width,
            glass.sampleMargin / scene.height, glass.width / scene.width, glass.height / scene.height)
        property color surfaceColor: glass.surfaceColor
        property real materialOpacity: glass.materialOpacity
        property real darkAmount: glass.dark ? 1 : 0
        property real backdropWeight: glass.hasBackdrop && scene.status === Image.Ready ? 1 : 0
        property real cornerRadius: 13
        property point lightPoint: Qt.point(0.25, 0.08)
        fragmentShader: "qrc:/shaders/glass-refract.frag.qsb"
    }
}
