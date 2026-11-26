import QtQuick
import "ui" as Pet

QtObject {
    id: root
    required property QtObject pluginApi
    property Pet.PetController controller: Pet.PetController {
        id: controller
        pluginApi: root.pluginApi
        onChanged: root.pluginApi.publish("pet.state", controller.snapshot())
    }
    property Connections bridge: Connections {
        target: root.pluginApi
        function onLifecycleEvent(phase, reason) {
            if (phase !== "started") return
            root.pluginApi.showWindow("pet")
            root.pluginApi.publish("pet.state", root.controller.snapshot())
        }
        function onCommandReceived(requestId, command, payload) {
            const action = payload.action === undefined ? "pet" : payload.action
            if (command !== "interact") {
                root.pluginApi.reply(requestId, false, {}, qsTr("桌宠不认识这个动作。"))
                return
            }
            if (action === "pet") root.controller.pet()
            else if (action === "drag" && typeof payload.active === "boolean") root.controller.drag(payload.active)
            else if (action === "finished" && typeof payload.name === "string"
                     && Number.isInteger(payload.interactionId)) root.controller.finish(payload.name, payload.interactionId)
            else if (action !== "snapshot") {
                root.pluginApi.reply(requestId, false, {}, qsTr("桌宠不认识这个动作。"))
                return
            }
            root.pluginApi.reply(requestId, true, root.controller.snapshot())
            root.pluginApi.publish("pet.state", root.controller.snapshot())
        }
    }
}
