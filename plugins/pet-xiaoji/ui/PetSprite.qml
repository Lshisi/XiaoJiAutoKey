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
    property real breath: 0
    property real sway: 0
    readonly property var actionData: root.skin.actions[root.action] || root.skin.actions.idle
    readonly property var sheet: root.actionData.sheet ? root.skin.sheets[root.actionData.sheet] : root.skin
    readonly property var frame: root.actionData.frames[Math.min(root.frameIndex, root.actionData.frames.length - 1)]
    readonly property bool failed: atlas.status === Image.Error
    readonly property bool ready: atlas.status === Image.Ready
    readonly property bool playing: root.ready && root.active && root.visible && root.motionEnabled
        && !root.finished && root.actionData.frames.length > 1
    signal animationFinished(string action, int interactionId)
    implicitWidth: 96
    implicitHeight: 96
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

    Item {
        anchors.fill: parent
        clip: true
        transform: [
            Scale {
                origin.x: root.width * 0.5; origin.y: root.height * 0.94
                xScale: 1 + root.breath * 0.3; yScale: 1 - root.breath
            },
            Rotation { origin.x: root.width * 0.5; origin.y: root.height * 0.94; angle: root.sway }
        ]
        Image {
            id: atlas
            x: -root.frame.column * root.width
            y: -root.frame.row * root.height
            width: root.width * root.sheet.columns
            height: root.height * root.sheet.rows
            source: root.sheet.source
            sourceSize: Qt.size(root.sheet.frameWidth * root.sheet.columns, root.sheet.frameHeight * root.sheet.rows)
            asynchronous: true
            smooth: true
            mipmap: true
        }
    }
    PetSleepMarks {
        anchors.fill: parent
        sleeping: root.action === "sleep"
        active: root.active && root.visible
        motionEnabled: root.motionEnabled
    }
    SequentialAnimation {
        running: root.ready && root.active && root.visible && root.motionEnabled && root.action === "practice"
        loops: Animation.Infinite
        onRunningChanged: if (!running) root.sway = 0
        NumberAnimation { target: root; property: "sway"; from: 0; to: -1.5; duration: 360; easing.type: Easing.InOutSine }
        NumberAnimation { target: root; property: "sway"; to: 1.5; duration: 360; easing.type: Easing.InOutSine }
        NumberAnimation { target: root; property: "sway"; to: 0; duration: 360; easing.type: Easing.InOutSine }
    }
    SequentialAnimation {
        running: root.ready && root.active && root.visible && root.motionEnabled
            && (root.action === "idle" || root.action === "sleep" || root.action === "rest")
        loops: Animation.Infinite
        onRunningChanged: if (!running) root.breath = 0
        NumberAnimation { target: root; property: "breath"; from: 0; to: root.action === "sleep" ? 0.035 : 0.015; duration: 1500; easing.type: Easing.InOutSine }
        NumberAnimation { target: root; property: "breath"; to: 0; duration: 1500; easing.type: Easing.InOutSine }
    }
    Timer {
        id: frameClock
        repeat: false
        onTriggered: root.advance()
    }
}
