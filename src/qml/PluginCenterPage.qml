import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import XiaoJi.UI

Item {
    id: root
    readonly property QtObject page: pluginCenterModel
    property int tab: 2
    property var detail: null
    readonly property var rows: {
        const input = tab === 1 ? page.state.discover : page.state.installed
        const query = search.text.trim().toLowerCase()
        return input.filter(row => (tab !== 0 || row.builtin)
            && (!query.length || (row.name + " " + row.author + " " + row.id).toLowerCase().indexOf(query) >= 0)
            && (kind.currentIndex === 0 || (kind.currentIndex === 1 ? row.hasDriver : kind.currentIndex === 2 ? row.hasPage : kind.currentIndex === 3 ? row.hasBackground : row.hasWindow))
            && (publisher.currentIndex === 0 || (publisher.currentIndex === 1 ? row.official : !row.official))
            && (review.currentIndex === 0 || (review.currentIndex === 1 ? row.reviewed : !row.reviewed)))
    }
    QuietCard {
        anchors.fill: parent
        anchors.leftMargin: Theme.pageMargin
        anchors.rightMargin: Theme.pageMargin
        anchors.topMargin: Theme.cardGap
        anchors.bottomMargin: Theme.cardGap
        ColumnLayout {
            anchors.fill: parent
            spacing: 10
            RowLayout {
                Layout.fillWidth: true
                spacing: 6
                QuietHeading { text: qsTr("插件中心"); pageTitle: true; Layout.fillWidth: true }
                ActionButton { text: qsTr("导入"); leadingIcon: "import"; enabled: !root.page.state.busy; onClicked: root.page.dispatch("import") }
                IconButton { iconName: "refresh"; hint: qsTr("检查插件更新"); enabled: !root.page.state.busy; onClicked: root.page.dispatch("refresh") }
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                Repeater {
                    model: [qsTr("内置"), qsTr("发现"), qsTr("已安装")]
                    delegate: ActionButton {
                        required property string modelData
                        required property int index
                        text: modelData
                        quiet: true
                        accented: root.tab === index
                        onClicked: root.tab = index
                    }
                }
                Item { Layout.fillWidth: true }
                QuietLabel { visible: root.page.state.updates > 0; text: qsTr("%1 个更新").arg(root.page.state.updates); color: Theme.accent }
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                QuietTextField { id: search; Layout.fillWidth: true; Layout.minimumWidth: 80; placeholderText: qsTr("搜索名称或作者"); Accessible.name: qsTr("搜索插件") }
                QuietCombo { id: publisher; Layout.preferredWidth: 88; model: [qsTr("全部来源"), qsTr("官方"), qsTr("社区")]; Accessible.name: qsTr("插件发布者") }
                QuietCombo { id: kind; Layout.preferredWidth: 88; model: [qsTr("全部类型"), qsTr("驱动"), qsTr("页面"), qsTr("后台"), qsTr("窗口")]; Accessible.name: qsTr("插件类型") }
                QuietCombo { id: review; Layout.preferredWidth: 88; model: [qsTr("全部审核"), qsTr("已审核"), qsTr("未审核")]; Accessible.name: qsTr("审核状态") }
            }
            ListView {
                id: list
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                model: root.rows
                ScrollBar.vertical: QuietScrollBar {}
                delegate: PluginRow {
                    required property var modelData
                    width: list.width
                    record: modelData
                    discovery: root.tab === 1
                    busy: root.page.state.busy
                    onAction: (operation, pluginId, value) => {
                        if (operation === "details") root.detail = value
                        else root.page.dispatch(operation, pluginId, value)
                    }
                }
                QuietLabel {
                    anchors.centerIn: parent
                    width: parent.width
                    horizontalAlignment: Text.AlignHCenter
                    visible: list.count === 0
                    text: root.tab === 1 && !root.page.state.registryConfigured ? qsTr("插件目录尚未就绪，暂时可通过“导入”安装插件。") : qsTr("没有符合条件的插件。")
                    color: Theme.muted
                }
            }
            QuietLabel { Layout.fillWidth: true; text: root.page.state.status || qsTr("未审核插件也可由你选择安装，更新不会强制应用。"); color: Theme.muted; maximumLineCount: 3; elide: Text.ElideRight }
        }
    }
    Loader {
        active: root.detail !== null
        sourceComponent: PluginDetails {
            parent: Overlay.overlay
            record: {
                if (!root.detail) return ({})
                const installed = root.page.state.installed.find(row => row.id === root.detail.id)
                return installed || root.detail
            }
            page: root.page
            Component.onCompleted: open()
            onDismissed: root.detail = null
        }
    }
}
