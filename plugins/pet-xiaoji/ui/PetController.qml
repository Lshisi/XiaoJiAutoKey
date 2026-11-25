import QtQuick

QtObject {
    id: root
    required property QtObject pluginApi
    property bool sleeping: false
    property bool dragging: false
    property string gesture: ""
    property bool initialized: false
    property bool observedRun: false
    readonly property bool linked: root.pluginApi.settings.linkRunning !== false
    readonly property bool engineRunning: root.pluginApi.state.repeatRunning === true || root.pluginApi.state.macroRunning === true
    readonly property bool engineActive: root.pluginApi.state.repeatActive === true || root.pluginApi.state.macroActive === true
        || (root.pluginApi.state.repeatActive === undefined && root.pluginApi.state.repeatRunning === true)
        || (root.pluginApi.state.macroActive === undefined && root.pluginApi.state.macroRunning === true)
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
        return { action: root.action, statusText: root.statusText }
    }
    function play(name) {
        root.sleeping = false
        root.gesture = name
        root.gestureTimer.restart()
    }
    function pet() {
        if (!root.dragging) root.play("pet")
    }
    function drag(active) {
        root.dragging = active
        root.sleeping = false
        root.gesture = ""
        root.gestureTimer.stop()
        if (active) root.dragGuard.restart()
        else {
            root.dragGuard.stop()
            root.play("land")
        }
    }
    onActionChanged: root.changed()
    onStatusTextChanged: root.changed()
    onEngineRunningChanged: {
        if (!root.initialized || !root.linked) return
        root.sleeping = false
        if (root.engineRunning) {
            root.observedRun = true
            root.gesture = ""
            root.gestureTimer.stop()
        } else if (root.observedRun) {
            root.observedRun = false
            root.play("finish")
        }
    }
    onEngineActiveChanged: {
        if (root.initialized && root.linked && root.engineActive) {
            root.sleeping = false
            root.gesture = ""
            root.gestureTimer.stop()
        }
    }
    onLinkedChanged: {
        if (!root.initialized) return
        root.observedRun = root.linked && root.engineRunning
        root.sleeping = false
        root.gesture = ""
        root.gestureTimer.stop()
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
    property Timer gestureTimer: Timer {
        interval: root.gesture === "land" ? 400 : 1000
        onTriggered: root.gesture = ""
    }
    property Timer dragGuard: Timer {
        interval: 30000
        onTriggered: root.drag(false)
    }
}
