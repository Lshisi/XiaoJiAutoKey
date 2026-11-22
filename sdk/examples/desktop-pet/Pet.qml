import QtQuick
import XiaoJi.UI

Item {
    id: root
    required property QtObject pluginApi
    required property string windowId
    property int greetings: 0
    readonly property bool windowVisible: Window.window !== null && Window.window.visible
    Connections {
        target: root.pluginApi
        function onEventReceived(topic, payload) { if (topic === "pet.state") root.greetings = payload.greetings || 0 }
    }
    DragHandler {
        target: null
        onActiveChanged: if (active) root.pluginApi.startWindowMove(root.windowId)
    }
    TapHandler {
        acceptedButtons: Qt.RightButton
        onTapped: root.pluginApi.request("poke")
    }
    Rectangle {
        id: pet
        x: (root.width - width) / 2
        y: 24
        width: 100
        height: 96
        radius: 38
        color: Theme.accentSoft
        border.color: Theme.accentBorder
        Accessible.role: Accessible.Button
        Accessible.name: qsTr("桌宠，拖动移动，右键打招呼")
        activeFocusOnTab: true
        Keys.onReturnPressed: root.pluginApi.request("poke")
        Rectangle { x: 25; y: 36; width: 8; height: 12; radius: 4; color: Theme.text; Accessible.ignored: true }
        Rectangle { x: 66; y: 36; width: 8; height: 12; radius: 4; color: Theme.text; Accessible.ignored: true }
        Rectangle { anchors.horizontalCenter: parent.horizontalCenter; y: 61; width: 16; height: 4; radius: 2; color: Theme.accent; Accessible.ignored: true }
        SequentialAnimation on y {
            running: root.windowVisible && root.pluginApi.runtime.backgroundRunning === true && Theme.motionEnabled
            loops: Animation.Infinite
            NumberAnimation { to: 30; duration: 1000; easing.type: Easing.InOutSine }
            NumberAnimation { to: 24; duration: 1000; easing.type: Easing.InOutSine }
        }
    }
    QuietLabel {
        anchors.horizontalCenter: parent.horizontalCenter
        y: 134
        width: root.width - 16
        horizontalAlignment: Text.AlignHCenter
        text: qsTr("%1 · 收到 %2 次招呼").arg(root.pluginApi.settings.petName || qsTr("小叽")).arg(root.greetings)
    }
    IconButton { anchors.right: parent.right; anchors.top: parent.top; iconName: "close"; hint: qsTr("关闭桌宠窗口"); onClicked: root.pluginApi.closeWindow(root.windowId) }
}
