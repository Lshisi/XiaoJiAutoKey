import QtQuick

Item {
    id: root
    required property var skin
    property string action: "idle"
    property int interactionId: 0
    property bool active: true
    property bool motionEnabled: true
    property int frameIndex: 0
    property bool finished: false
    property bool initialized: false
    readonly property var actionData: root.skin.actions[root.action]
    readonly property var frame: root.actionData.frames[Math.min(root.frameIndex, root.actionData.frames.length - 1)]
    readonly property bool failed: atlas.status === Image.Error
    readonly property bool ready: atlas.status === Image.Ready
    readonly property bool playing: root.ready && root.active && root.visible && root.motionEnabled
        && !root.finished && root.actionData.frames.length > 1
    signal animationFinished(string action, int interactionId)
    implicitWidth: 192
    implicitHeight: 192
    clip: true
    Accessible.ignored: true

    function reset() {
        frameClock.stop()
        root.frameIndex = 0
        root.finished = false
        Qt.callLater(root.schedule)
    }
    function schedule() {
        frameClock.stop()
        if (root.playing) {
            frameClock.interval = root.frame.duration
            frameClock.start()
        } else if (!root.motionEnabled && root.ready && root.active && root.visible
                   && !root.finished && root.actionData.loopFrom < 0 && root.actionData.frames.length > 1) {
            root.complete()
        }
    }
    function complete() {
        root.finished = true
        root.animationFinished(root.action, root.interactionId)
    }
    function advance() {
        if (root.frameIndex + 1 < root.actionData.frames.length) root.frameIndex += 1
        else if (root.actionData.loopFrom >= 0) root.frameIndex = root.actionData.loopFrom
        else root.complete()
        root.schedule()
    }
    onActionDataChanged: if (root.initialized) root.reset()
    onInteractionIdChanged: if (root.initialized) root.reset()
    onMotionEnabledChanged: if (root.initialized) root.reset()
    onPlayingChanged: Qt.callLater(root.schedule)
    onReadyChanged: Qt.callLater(root.schedule)
    onActiveChanged: Qt.callLater(root.schedule)
    onVisibleChanged: Qt.callLater(root.schedule)
    Component.onCompleted: {
        root.initialized = true
        root.reset()
    }

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
        repeat: false
        onTriggered: root.advance()
    }
}
