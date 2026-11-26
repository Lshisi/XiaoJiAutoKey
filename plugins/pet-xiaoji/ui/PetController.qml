import QtQuick

QtObject {
    id: root
    required property QtObject pluginApi
    property bool sleeping: false
    property bool dragging: false
    property string gesture: ""
    property int interactionId: 0
    property bool initialized: false
    property bool observedRun: false
    property double idleSince: Date.now()
    property int idleStage: 0
    readonly property bool linked: root.pluginApi.settings.linkRunning !== false
    readonly property bool engineRunning: root.pluginApi.state.repeatRunning === true || root.pluginApi.state.macroRunning === true
    readonly property bool engineActive: root.pluginApi.state.repeatActive === true || root.pluginApi.state.macroActive === true
    readonly property bool live: root.pluginApi.runtime.backgroundRunning === true
    readonly property bool windowVisible: {
        const windows = root.pluginApi.runtime.windows || []
        for (let i = 0; i < windows.length; ++i)
            if (windows[i].id === "pet") return windows[i].visible === true
        return false
    }
    readonly property string action: root.dragging ? "drag" : root.gesture.length ? root.gesture
        : root.linked && root.engineRunning ? (root.engineActive ? "practice" : "rest") : root.sleeping ? "sleep" : "idle"
    readonly property string statusText: root.dragging ? qsTr("被拎起来啦")
        : root.gesture === "pet" ? qsTr("摸摸头，开心叽")
        : root.gesture === "land" ? qsTr("站稳啦")
        : root.gesture === "finish" ? qsTr("收剑，收工！")
        : root.gesture === "look" ? qsTr("看看你在忙什么")
        : root.gesture === "stretch" ? qsTr("伸个懒腰")
        : root.gesture === "yawn" ? qsTr("有点困了")
        : root.gesture === "wake" ? qsTr("醒来陪你啦")
        : root.action === "practice" ? qsTr("认真练剑中")
        : root.action === "rest" ? (root.pluginApi.state.gameGatePaused ? qsTr("等你回到游戏") : qsTr("暂停，驻剑等候"))
        : root.sleeping ? qsTr("小憩一会儿") : qsTr("陪你待着")
    readonly property bool resting: root.live && !(root.linked && root.engineRunning)
        && !root.dragging && !root.sleeping
    signal changed()

    function snapshot() {
        return { action: root.action, interactionId: root.interactionId, statusText: root.statusText }
    }
    function resetIdle() {
        root.idleSince = Date.now()
        root.idleStage = 0
    }
    function play(name, interaction = true) {
        if (interaction) root.resetIdle()
        root.sleeping = false
        root.interactionId += 1
        root.gesture = name
    }
    function finish(name, interactionId) {
        if (name === root.gesture && interactionId === root.interactionId) root.gesture = ""
    }
    function pet() {
        if (!root.dragging) root.play(root.sleeping ? "wake" : "pet")
    }
    function drag(active) {
        root.dragging = active
        root.resetIdle()
        root.sleeping = false
        root.gesture = ""
        if (active) root.dragGuard.restart()
        else {
            root.dragGuard.stop()
            root.play("land")
        }
    }
    function idleTick() {
        if (root.gesture.length) return
        const elapsed = Date.now() - root.idleSince
        if (elapsed >= 120000) root.sleeping = true
        else if (elapsed >= 95000 && root.idleStage < 3) {
            root.idleStage = 3
            root.play("yawn", false)
        } else if (elapsed >= 60000 && root.idleStage < 2) {
            root.idleStage = 2
            root.play("stretch", false)
        } else if (elapsed >= 25000 && root.idleStage < 1) {
            root.idleStage = 1
            root.play("look", false)
        }
    }
    onActionChanged: root.changed()
    onInteractionIdChanged: Qt.callLater(root.changed)
    onStatusTextChanged: root.changed()
    onEngineRunningChanged: {
        if (!root.initialized || !root.linked) return
        const wasSleeping = root.sleeping
        root.sleeping = false
        root.resetIdle()
        if (root.engineRunning) {
            root.observedRun = true
            if (wasSleeping) root.play("wake")
            else if (root.gesture !== "wake") root.gesture = ""
        } else if (root.observedRun) {
            root.observedRun = false
            root.play("finish")
        }
    }
    onEngineActiveChanged: {
        if (root.initialized && root.linked && root.engineActive) {
            const wasSleeping = root.sleeping
            root.sleeping = false
            root.resetIdle()
            if (wasSleeping) root.play("wake")
            else if (root.gesture !== "wake") root.gesture = ""
        }
    }
    onLinkedChanged: {
        if (!root.initialized) return
        root.observedRun = root.linked && root.engineRunning
        root.resetIdle()
        root.sleeping = false
        root.gesture = ""
    }
    onLiveChanged: {
        if (!root.initialized) return
        root.resetIdle()
        if (!root.live) {
            root.dragGuard.stop()
            root.dragging = false
            root.sleeping = false
            root.gesture = ""
        }
    }
    onWindowVisibleChanged: {
        if (!root.initialized) return
        if (!root.windowVisible && ["look", "stretch", "yawn", "wake"].indexOf(root.gesture) >= 0) root.gesture = ""
        else if (root.windowVisible && root.resting) root.idleTick()
    }
    Component.onCompleted: {
        root.observedRun = root.linked && root.engineRunning
        root.resetIdle()
        root.initialized = true
    }
    property Timer idleClock: Timer {
        interval: 1000
        repeat: true
        running: root.resting && root.windowVisible
        onTriggered: root.idleTick()
    }
    property Timer dragGuard: Timer {
        interval: 30000
        onTriggered: root.drag(false)
    }
}
