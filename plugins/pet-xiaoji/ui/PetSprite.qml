import QtQuick

Item {
    id: root
    required property var skin
    property string action: "idle"
    property bool active: true
    property bool motionEnabled: true
    property int frameIndex: 0
    property bool finished: false
    readonly property var actionData: root.skin.actions[root.action]
    readonly property var frame: root.actionData.frames[Math.min(root.frameIndex, root.actionData.frames.length - 1)]
    readonly property bool failed: atlas.status === Image.Error
    readonly property bool ready: atlas.status === Image.Ready
    readonly property bool playing: root.ready && root.active && root.visible && root.motionEnabled
        && !root.finished && root.actionData.frames.length > 1
    implicitWidth: 192
    implicitHeight: 192
    clip: true
    Accessible.ignored: true

    function reset() {
        root.frameIndex = 0
        root.finished = false
        Qt.callLater(function() { if (root.playing) frameClock.restart() })
    }
    function advance() {
        if (root.frameIndex + 1 < root.actionData.frames.length) root.frameIndex += 1
        else if (root.actionData.loopFrom >= 0) root.frameIndex = root.actionData.loopFrom
        else root.finished = true
    }
    onActionChanged: root.reset()
    onSkinChanged: root.reset()
    onMotionEnabledChanged: if (!root.motionEnabled) root.reset()

    Image {
        id: atlas
        x: -root.frame.column * root.width
        y: -root.frame.row * root.height
        width: root.width * root.skin.columns
        height: root.height * root.skin.rows
        source: root.skin.source
        sourceSize: Qt.size(root.skin.frameWidth * root.skin.columns, root.skin.frameHeight * root.skin.rows)
        asynchronous: true
        smooth: false
    }
    Timer {
        id: frameClock
        interval: root.frame.duration
        repeat: true
        running: root.playing
        onTriggered: root.advance()
    }
}
