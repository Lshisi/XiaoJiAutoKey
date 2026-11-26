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
    readonly property bool linked: root.pluginApi.settings.linkRunning !== false
    readonly property bool engineRunning: root.pluginApi.state.repeatRunning === true || root.pluginApi.state.macroRunning === true
    readonly property bool engineActive: root.pluginApi.state.repeatActive === true || root.pluginApi.state.macroActive === true
    readonly property bool live: root.pluginApi.runtime.backgroundRunning === true
    readonly property string action: root.dragging ? "drag" : root.gesture.length ? root.gesture
        : root.linked && root.engineRunning ? (root.engineActive ? "practice" : "rest") : root.sleeping ? "sleep" : "idle"
    readonly property string statusText: root.dragging ? qsTr("被拎起来啦")
        : root.gesture === "pet" ? qsTr("摸摸头，开心叽")
        : root.gesture === "land" ? qsTr("站稳啦")
        : root.gesture === "finish" ? qsTr("收剑，收工！")
        : root.action === "practice" ? qsTr("认真练剑中")
        : root.action === "rest" ? (root.pluginApi.state.gameGatePaused ? qsTr("等你回到游戏") : qsTr("暂停，驻剑等候"))
        : root.sleeping ? qsTr("小憩一会儿") : qsTr("陪你待着")
    readonly property bool canSleep: root.live && !(root.linked && root.engineRunning)
        && !root.dragging && !root.gesture.length && !root.sleeping
    signal changed()

    function snapshot() {
        return { action: root.action, interactionId: root.interactionId, statusText: root.statusText }
    }
    function play(name) {
        root.sleeping = false
        root.interactionId += 1
        root.gesture = name
    }
    function finish(name, interactionId) {
        if (name === root.gesture && interactionId === root.interactionId) root.gesture = ""
    }
    function pet() {
        if (!root.dragging) root.play("pet")
    }
    function drag(active) {
        root.dragging = active
        root.sleeping = false
        root.gesture = ""
        if (active) root.dragGuard.restart()
        else {
            root.dragGuard.stop()
            root.play("land")
        }
    }
    onActionChanged: root.changed()
    onInteractionIdChanged: Qt.callLater(root.changed)
    onStatusTextChanged: root.changed()
    onEngineRunningChanged: {
        if (!root.initialized || !root.linked) return
        root.sleeping = false
        if (root.engineRunning) {
            root.observedRun = true
            root.gesture = ""
        } else if (root.observedRun) {
            root.observedRun = false
            root.play("finish")
        }
    }
    onEngineActiveChanged: {
        if (root.initialized && root.linked && root.engineActive) {
            root.sleeping = false
            root.gesture = ""
        }
    }
    onLinkedChanged: {
        if (!root.initialized) return
        root.observedRun = root.linked && root.engineRunning
        root.sleeping = false
        root.gesture = ""
    }
    Component.onCompleted: {
        root.observedRun = root.linked && root.engineRunning
        root.initialized = true
    }
    property Timer sleepTimer: Timer {
        interval: 120000
        running: root.canSleep
        onTriggered: root.sleeping = true
    }
    property Timer dragGuard: Timer {
        interval: 30000
        onTriggered: root.drag(false)
    }
}
