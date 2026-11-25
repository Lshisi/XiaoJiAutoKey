import QtQuick

Item {
    id: root
    required property var skin
    property string action: "idle"
    property bool active: true
    property bool motionEnabled: true
    property bool blinking: false
    readonly property var clip: root.skin.actions[root.action] || root.skin.actions.idle
    readonly property bool idleStill: root.action === "idle" && !root.blinking
    readonly property bool loops: root.action === "practice" || root.action === "sleep" || root.action === "drag"
    readonly property bool failed: imageProbe.status === Image.Error
    readonly property bool ready: imageProbe.status === Image.Ready
    implicitWidth: 192
    implicitHeight: 192
    Accessible.ignored: true
    onActionChanged: {
        root.blinking = false
        Qt.callLater(function() {
            sprite.currentFrame = 0
            if (root.ready && root.motionEnabled && !root.idleStill) sprite.restart()
        })
    }
    onMotionEnabledChanged: if (!root.motionEnabled) Qt.callLater(function() { sprite.currentFrame = 0 })
    Image {
        id: imageProbe
        visible: false
        source: root.skin.source
        sourceSize: Qt.size(root.skin.frameWidth * root.skin.columns, root.skin.frameHeight * root.skin.rows)
        asynchronous: true
    }
    AnimatedSprite {
        id: sprite
        anchors.fill: parent
        source: root.ready ? root.skin.source : ""
        frameWidth: root.skin.frameWidth
        frameHeight: root.skin.frameHeight
        frameX: root.clip.frame * root.skin.frameWidth
        frameY: root.clip.row * root.skin.frameHeight
        frameCount: root.idleStill || !root.motionEnabled ? 1 : root.clip.count
        frameDuration: root.clip.duration
        loops: root.loops ? AnimatedSprite.Infinite : 1
        finishBehavior: AnimatedSprite.FinishAtFinalFrame
        interpolate: false
        smooth: false
        running: root.ready && root.motionEnabled && !root.idleStill
        paused: !root.active || !root.visible
        onFinished: if (root.action === "idle") root.blinking = false
    }
    Timer {
        interval: 4500
        repeat: true
        running: root.active && root.visible && root.ready && root.motionEnabled && root.action === "idle"
        onTriggered: {
            root.blinking = true
            sprite.restart()
        }
    }
}
