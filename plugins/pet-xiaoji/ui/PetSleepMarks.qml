import QtQuick

Item {
    id: root
    property bool sleeping: false
    property bool active: true
    property bool motionEnabled: true
    visible: root.sleeping && root.active
    Accessible.ignored: true

    Repeater {
        model: 3
        delegate: Text {
            id: mark
            required property int index
            property real drift: 0
            opacity: 0
            x: root.width * (0.62 + index * 0.09)
            y: root.height * (0.25 - index * 0.055) + drift
            text: "z"
            font.pixelSize: Math.max(9, root.width * (0.11 + index * 0.015))
            font.weight: Font.DemiBold
            color: "#fff3d3"
            style: Text.Outline
            styleColor: "#75604a"
            Accessible.ignored: true

            Binding { target: mark; property: "opacity"; value: root.motionEnabled ? 0 : 0.85; when: !floating.running }
            Binding { target: mark; property: "drift"; value: 0; when: !floating.running }
            SequentialAnimation {
                id: floating
                running: root.visible && root.motionEnabled
                loops: Animation.Infinite
                PauseAnimation { duration: mark.index * 280 }
                ParallelAnimation {
                    NumberAnimation { target: mark; property: "drift"; from: 0; to: -root.height * 0.1; duration: 1800; easing.type: Easing.InOutSine }
                    SequentialAnimation {
                        NumberAnimation { target: mark; property: "opacity"; from: 0; to: 0.85; duration: 350 }
                        PauseAnimation { duration: 700 }
                        NumberAnimation { target: mark; property: "opacity"; to: 0; duration: 750 }
                    }
                }
            }
        }
    }
}
