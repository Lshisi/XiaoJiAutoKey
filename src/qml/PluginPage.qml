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
