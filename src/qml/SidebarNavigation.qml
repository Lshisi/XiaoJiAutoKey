import QtQuick
import QtQuick.Layouts
import XiaoJi.UI

ColumnLayout {
    id: navigation
    readonly property var pluginState: pluginCenterModel.state
    readonly property bool inPluginPage: appView.currentPage.indexOf("plugin:") === 0
    spacing: 2
    Repeater {
        model: [
            { page: "keys", title: qsTr("按键连发"), icon: "keyboard" },
            { page: "macro", title: qsTr("循环连发"), icon: "repeat" },
            { page: "usage", title: qsTr("使用统计"), icon: "bar-chart" },
            { page: "driver", title: qsTr("驱动管理"), icon: "monitor" }
        ]
        delegate: SidebarEntry {
            required property var modelData
            Layout.fillWidth: true
            Layout.leftMargin: 6
            Layout.rightMargin: 6
            pageId: modelData.page
            text: modelData.title
            iconName: modelData.icon
        }
    }
    RowLayout {
        Layout.fillWidth: true
        Layout.leftMargin: 6
        Layout.rightMargin: 6
        spacing: 0
        SidebarEntry {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            pageId: "plugins"
            text: qsTr("插件中心")
            iconName: "puzzle"
            selected: appView.currentPage === "plugins" || (navigation.inPluginPage && !navigation.pluginState.expanded)
        }
        IconButton {
            visible: navigation.pluginState.pages.length > 0
            compact: true
            iconName: "chevron-down"
            rotation: navigation.pluginState.expanded ? 0 : -90
            hint: navigation.pluginState.expanded ? qsTr("收起插件页面") : qsTr("展开插件页面")
            onClicked: pluginCenterModel.dispatch("expanded", "", !navigation.pluginState.expanded)
        }
    }
    ColumnLayout {
        visible: navigation.pluginState.expanded && navigation.pluginState.pages.length > 0
        Layout.fillWidth: true
        Layout.leftMargin: 18
        Layout.rightMargin: 6
        spacing: 2
        Repeater {
            model: navigation.pluginState.pages
            delegate: SidebarEntry {
                required property var modelData
                Layout.fillWidth: true
                pageId: modelData.id
                text: modelData.title
                iconName: modelData.icon
                nested: true
            }
        }
    }
    Repeater {
        model: [
            { page: "settings", title: qsTr("全局设置"), icon: "settings" },
            { page: "about", title: qsTr("关于软件"), icon: "info" }
        ]
        delegate: SidebarEntry {
            required property var modelData
            Layout.fillWidth: true
            Layout.leftMargin: 6
            Layout.rightMargin: 6
            pageId: modelData.page
            text: modelData.title
            iconName: modelData.icon
        }
    }
}
