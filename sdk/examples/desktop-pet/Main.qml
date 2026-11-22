import QtQuick
import QtQuick.Layouts
import XiaoJi.UI

QuietCard {
    id: root
    required property QtObject pluginApi
    property int seconds: 0
    property int greetings: 0
    property string feedback: ""
    property string pending: ""
    function apply(payload) {
        root.seconds = payload.seconds || 0
        root.greetings = payload.greetings || 0
    }
    Connections {
        target: root.pluginApi
        function onEventReceived(topic, payload) { if (topic === "pet.state") root.apply(payload) }
        function onRequestFinished(requestId, ok, result, error) {
            if (requestId !== root.pending) return
            root.pending = ""
            root.feedback = ok ? qsTr("桌宠已回应") : error
            if (ok) root.apply(result)
        }
    }
    Component.onCompleted: {
        if (root.pluginApi.runtime.backgroundRunning) root.pending = root.pluginApi.request("snapshot")
    }
    ColumnLayout {
        anchors.fill: parent
        spacing: Theme.cardGap
        QuietHeading { text: qsTr("桌宠设置"); pageTitle: true; Layout.fillWidth: true }
        QuietLabel { text: qsTr("后台独立于设置页运行。切换页面或最小化小叽后，桌宠继续活动。"); color: Theme.muted; Layout.fillWidth: true }
        QuietLabel { text: root.pluginApi.runtime.backgroundRunning ? qsTr("已运行 %1 秒 · 打过 %2 次招呼").arg(root.seconds).arg(root.greetings) : qsTr("后台未运行"); Layout.fillWidth: true }
        RowLayout {
            Layout.fillWidth: true
            ActionButton {
                text: root.pluginApi.runtime.backgroundRunning ? qsTr("停止后台") : qsTr("启动后台")
                enabled: root.pluginApi.runtime.status !== "starting" && root.pluginApi.runtime.status !== "stopping"
                onClicked: root.pluginApi.setBackgroundRunning(!root.pluginApi.runtime.backgroundRunning)
            }
            ActionButton { text: qsTr("显示桌宠"); onClicked: root.pluginApi.showWindow("pet") }
            ActionButton { text: qsTr("隐藏桌宠"); onClicked: root.pluginApi.hideWindow("pet") }
        }
        QuietLabel { text: qsTr("桌宠名称") }
        RowLayout {
            Layout.fillWidth: true
            QuietTextField { id: nameField; Layout.fillWidth: true; maximumLength: 24; text: root.pluginApi.settings.petName || ""; placeholderText: qsTr("小叽"); Accessible.name: qsTr("桌宠名称") }
            ActionButton { text: qsTr("保存"); onClicked: root.pluginApi.saveSetting("petName", nameField.text.trim()) }
        }
        RowLayout {
            Layout.fillWidth: true
            ActionButton { text: qsTr("打个招呼"); enabled: root.pluginApi.runtime.backgroundRunning === true && !root.pending.length; onClicked: root.pending = root.pluginApi.request("poke") }
            ActionButton { text: qsTr("恢复鼠标交互"); onClicked: root.pluginApi.configureWindow("pet", { clickThrough: false }) }
        }
        QuietLabel { text: root.feedback || root.pluginApi.runtime.error || qsTr("拖动桌宠可移动窗口，右键点击桌宠可打招呼。停止后台后不再随小叽恢复。"); color: Theme.muted; Layout.fillWidth: true }
        Item { Layout.fillHeight: true }
    }
}
