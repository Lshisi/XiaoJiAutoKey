import QtQuick
import QtQuick.Layouts
import XiaoJi.UI

QuietCard {
    id: root
    required property QtObject pluginApi
    property string pending: ""
    property string feedback: ""
    property int count: 0
    Connections {
        target: root.pluginApi
        function onRequestFinished(id, ok, result, error) {
            if (id !== root.pending) return
            root.pending = ""
            root.feedback = ok ? qsTr("原生后台已回复") : error
            if (ok) root.count = result.count || 0
        }
        function onEventReceived(topic, payload) { if (topic === "counter.changed") root.count = payload.count || 0 }
    }
    ColumnLayout {
        anchors.fill: parent
        spacing: Theme.cardGap
        QuietHeading { text: qsTr("原生后台通信"); pageTitle: true; Layout.fillWidth: true }
        QuietLabel { text: qsTr("当前计数：%1").arg(root.count); Layout.fillWidth: true }
        ActionButton {
            text: root.pluginApi.runtime.backgroundRunning ? qsTr("停止后台") : qsTr("启动后台")
            enabled: root.pluginApi.runtime.status !== "starting" && root.pluginApi.runtime.status !== "stopping"
            onClicked: root.pluginApi.setBackgroundRunning(!root.pluginApi.runtime.backgroundRunning)
        }
        ActionButton { text: qsTr("请求增加计数"); enabled: root.pluginApi.runtime.backgroundRunning === true && !root.pending.length; onClicked: root.pending = root.pluginApi.request("increment") }
        QuietLabel { text: root.feedback || root.pluginApi.runtime.error || ""; color: Theme.muted; Layout.fillWidth: true }
        Item { Layout.fillHeight: true }
    }
}
