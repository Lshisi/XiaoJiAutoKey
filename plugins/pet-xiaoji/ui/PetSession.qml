import QtQuick

QtObject {
    id: root
    required property QtObject pluginApi
    property string action: "idle"
    property int interactionId: 0
    property string statusText: ""
    property string pending: ""
    readonly property bool running: root.pluginApi.runtime.backgroundRunning === true
    function apply(payload) {
        root.action = payload.action || "idle"
        root.interactionId = payload.interactionId
        root.statusText = payload.statusText || ""
    }
    function refresh() {
        if (root.running && !root.pending.length) root.pending = root.pluginApi.request("interact", { action: "snapshot" })
    }
    onRunningChanged: {
        if (root.running) root.refresh()
        else {
            root.pending = ""
            root.apply({ action: "idle", interactionId: 0, statusText: "" })
        }
    }
    Component.onCompleted: root.refresh()
    property Connections bridge: Connections {
        target: root.pluginApi
        function onEventReceived(topic, payload) {
            if (topic === "pet.state") root.apply(payload)
        }
        function onRequestFinished(requestId, ok, result, error) {
            if (requestId !== root.pending) return
            root.pending = ""
            if (ok) root.apply(result)
        }
    }
}
