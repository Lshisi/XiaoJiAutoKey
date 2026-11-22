import QtQuick

QtObject {
    id: root
    required property QtObject pluginApi
    property int seconds: 0
    property int greetings: 0
    function snapshot() {
        return { seconds: root.seconds, greetings: root.greetings }
    }
    property Timer clock: Timer {
        interval: 1000
        repeat: true
        running: root.pluginApi.runtime.backgroundRunning === true
        onTriggered: {
            root.seconds += 1
            root.pluginApi.publish("pet.state", root.snapshot())
        }
    }
    property Connections bridge: Connections {
        target: root.pluginApi
        function onLifecycleEvent(phase, reason) {
            if (phase === "started") root.pluginApi.showWindow("pet")
        }
        function onCommandReceived(requestId, command, payload) {
            if (command === "poke") {
                root.greetings += 1
                root.pluginApi.publish("pet.state", root.snapshot())
            }
            root.pluginApi.reply(requestId, true, root.snapshot())
        }
    }
}
