import QtQuick
import QtQuick.Layouts
import XiaoJi.UI

QuietCard {
    id: root
    required property QtObject pluginApi
    ColumnLayout {
        anchors.fill: parent
        spacing: 14
        QuietHeading { text: root.pluginApi.settings.title || qsTr("运行状态面板"); pageTitle: true; Layout.fillWidth: true }
        QuietLabel { text: qsTr("由页面插件提供，状态来自小叽框架。"); color: Theme.muted; Layout.fillWidth: true }
        Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Theme.divider }
        RowLayout {
            Layout.fillWidth: true
            QuietLabel { text: qsTr("当前驱动"); color: Theme.muted }
            QuietLabel { text: root.pluginApi.state.driver || qsTr("未选择"); Layout.fillWidth: true; horizontalAlignment: Text.AlignRight }
        }
        RowLayout {
            Layout.fillWidth: true
            QuietLabel { text: qsTr("按键连发"); color: Theme.muted }
            QuietLabel { text: root.pluginApi.state.repeatRunning ? qsTr("运行中") : qsTr("未运行"); Layout.fillWidth: true; horizontalAlignment: Text.AlignRight }
        }
        RowLayout {
            Layout.fillWidth: true
            QuietLabel { text: qsTr("循环连发"); color: Theme.muted }
            QuietLabel { text: root.pluginApi.state.macroRunning ? qsTr("运行中") : qsTr("未运行"); Layout.fillWidth: true; horizontalAlignment: Text.AlignRight }
        }
        Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Theme.divider }
        QuietLabel { text: qsTr("显示标题") }
        RowLayout {
            Layout.fillWidth: true
            QuietTextField { id: titleField; Layout.fillWidth: true; text: root.pluginApi.settings.title || ""; placeholderText: qsTr("运行状态面板"); maximumLength: 40; Accessible.name: qsTr("插件显示标题") }
            ActionButton { text: qsTr("保存"); onClicked: root.pluginApi.saveSetting("title", titleField.text.trim()) }
        }
        Item { Layout.fillHeight: true }
        ActionButton { text: qsTr("前往按键连发"); onClicked: root.pluginApi.navigate("keys") }
    }
}
