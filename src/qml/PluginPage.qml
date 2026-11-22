import QtQuick
import QtQuick.Layouts
import XiaoJi.UI

Item {
    id: root
    readonly property var descriptor: {
        const pages = pluginCenterModel.state.pages
        return pages.find(page => page.id === appView.currentPage) || ({})
    }
    readonly property QtObject api: descriptor.id ? pluginCenterModel.contextForPage(descriptor.id) : null
    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: Theme.pageMargin
        anchors.rightMargin: Theme.pageMargin
        anchors.topMargin: Theme.cardGap
        anchors.bottomMargin: Theme.cardGap
        spacing: 8
        RowLayout {
            Layout.fillWidth: true
            ActionButton { text: qsTr("插件中心"); quiet: true; compact: true; onClicked: appView.selectPage("plugins") }
            QuietLabel { text: "/"; color: Theme.muted }
            QuietLabel { text: root.descriptor.title || ""; color: Theme.muted; Layout.fillWidth: true; Layout.minimumWidth: 0; elide: Text.ElideRight }
        }
        PluginPageHost {
            id: host
            Layout.fillWidth: true
            Layout.fillHeight: true
            source: root.descriptor.source || ""
            api: root.api
        }
        QuietLabel { visible: host.error.length > 0; text: host.error; color: Theme.danger; Layout.fillWidth: true }
    }
}
