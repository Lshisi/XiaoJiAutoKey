import QtQuick

Item {
    id: glass
    required property url backdrop
    required property bool hasBackdrop
    required property color surfaceColor
    required property real materialOpacity
    readonly property int sampleMargin: 12
    readonly property real backdropScale: 0.5
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
    ShaderEffectSource {
        id: backdropTexture
        sourceItem: scene
        smooth: true
        hideSource: true
        visible: false
        textureSize: Qt.size(Math.max(1, Math.ceil(scene.width * Screen.devicePixelRatio * glass.backdropScale)),
            Math.max(1, Math.ceil(scene.height * Screen.devicePixelRatio * glass.backdropScale)))
    }
    ShaderEffect {
        anchors.fill: parent
        property variant sourceTexture: backdropTexture
        property size surfaceSize: Qt.size(width, height)
        property vector4d sampleRegion: Qt.vector4d(glass.sampleMargin / scene.width,
            glass.sampleMargin / scene.height, glass.width / scene.width, glass.height / scene.height)
        property color surfaceColor: glass.surfaceColor
        property real materialOpacity: glass.materialOpacity
        property real backdropWeight: glass.hasBackdrop && scene.status === Image.Ready ? 1 : 0
        property real cornerRadius: 13
        property vector4d refractionCurve: Qt.vector4d(0.992, 2.332, 4.544, 6.923)
        // Match the reference at the existing default concentration of 70%.
        property real refractionPower: 1.779 * glass.materialOpacity / 0.7
        fragmentShader: "qrc:/shaders/glass-refract.frag.qsb"
    }
}
