import QtQuick
import XiaoJi.UI
import "ui" as Pet

Item {
    id: root
    required property QtObject pluginApi
    required property string windowId
    readonly property bool windowVisible: Window.window !== null && Window.window.visible && Window.window.visibility !== Window.Minimized
    readonly property int petSize: [160, 224, 288].indexOf(root.pluginApi.settings.size) >= 0 ? root.pluginApi.settings.size : 224
    function applySize() {
        if (root.windowVisible) root.pluginApi.configureWindow(root.windowId, { width: root.petSize, height: root.petSize })
    }
    Pet.PetSession { id: session; pluginApi: root.pluginApi }
    Pet.PetSkin {
        id: skin
        requestedId: root.pluginApi.settings.followFaction === true ? root.pluginApi.theme.factionId : root.pluginApi.settings.skinId || "cangjian"
    }
    onPetSizeChanged: root.applySize()
    onWindowVisibleChanged: {
        if (root.windowVisible) {
            session.refresh()
            root.applySize()
        }
    }
    Pet.PetSprite {
        id: character
        anchors.fill: parent
        skin: skin.current
        action: move.active ? "drag" : session.action
        interactionId: session.interactionId
        active: root.windowVisible && session.running
        motionEnabled: Theme.motionEnabled
        onAnimationFinished: function(name, interactionId) {
            if (session.running) root.pluginApi.request("interact", {
                action: "finished", name: name, interactionId: interactionId
            })
        }
    }
    Item {
        anchors.fill: parent
        anchors.margins: parent.width * 0.12
        Accessible.role: Accessible.Button
        Accessible.name: qsTr("%1小叽，点击摸头，拖动移动").arg(skin.current.name)
        Accessible.description: session.statusText
        DragHandler {
            id: move
            target: null
            acceptedButtons: Qt.LeftButton
            onActiveChanged: {
                if (!session.running) return
                root.pluginApi.request("interact", { action: "drag", active: active })
                if (active) root.pluginApi.startWindowMove(root.windowId)
            }
        }
        TapHandler {
            acceptedButtons: Qt.LeftButton
            onTapped: if (session.running) root.pluginApi.request("interact")
        }
        TapHandler {
            acceptedButtons: Qt.RightButton
            onTapped: root.pluginApi.navigate("plugin:xiaoji.pet.xiaoji/settings")
        }
    }
    QuietLabel {
        visible: character.failed
        anchors.centerIn: parent
        width: parent.width - 24
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.WordWrap
        text: qsTr("皮肤素材加载失败，请在插件中心重新安装桌宠。")
    }
}
