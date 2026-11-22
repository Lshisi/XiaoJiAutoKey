import QtQuick
import QtQuick.Layouts
import XiaoJi.UI

Item {
    id: root
    required property var record
    property bool discovery: false
    property bool busy: false
    signal action(string operation, string pluginId, var value)
    implicitHeight: content.implicitHeight + 22
    RowLayout {
        id: content
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        spacing: 10
        Item {
            Layout.preferredWidth: 32
            Layout.preferredHeight: 32
            QuietIcon { anchors.centerIn: parent; name: root.record.hasDriver ? "keyboard" : "puzzle"; color: Theme.muted; pixelSize: 22 }
        }
        ColumnLayout {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            spacing: 3
            RowLayout {
                Layout.fillWidth: true
                QuietLabel { text: root.record.name; font.weight: Theme.headingWeight; Layout.maximumWidth: Math.max(0, parent.width - identity.implicitWidth - (currentLabel.visible ? currentLabel.implicitWidth + 5 : 0) - 10); Layout.minimumWidth: 0; elide: Text.ElideRight; wrapMode: Text.NoWrap }
                QuietLabel { id: identity; text: root.record.builtin ? qsTr("内置") : root.record.official ? qsTr("官方") : root.record.source; color: Theme.muted }
                QuietLabel { id: currentLabel; visible: root.record.currentDriver === true; text: qsTr("当前使用"); color: Theme.accent }
                Item { Layout.fillWidth: true }
            }
            QuietLabel { Layout.fillWidth: true; text: root.record.description; color: Theme.muted; maximumLineCount: 2; elide: Text.ElideRight }
            QuietLabel {
                Layout.fillWidth: true
                text: qsTr("v%1 · %2 · %3").arg(root.record.version || "—").arg(root.record.author)
                    .arg(root.record.reviewed ? qsTr("此版本已审核") : root.record.builtin ? qsTr("随程序提供") : qsTr("未审核"))
                color: Theme.muted
                elide: Text.ElideRight
                wrapMode: Text.NoWrap
            }
            QuietLabel { visible: !!root.record.error && root.record.error.length > 0; Layout.fillWidth: true; text: root.record.error || ""; color: Theme.danger }
            QuietLabel { visible: !!root.record.disableReason; Layout.fillWidth: true; text: root.record.disableReason || ""; color: Theme.muted }
            QuietLabel { visible: !!root.record.removalError; Layout.fillWidth: true; text: root.record.removalError || ""; color: Theme.warning; maximumLineCount: 3; elide: Text.ElideRight }
            QuietLabel { visible: !!root.record.runtimeError; Layout.fillWidth: true; text: root.record.runtimeError || ""; color: Theme.danger; maximumLineCount: 2; elide: Text.ElideRight }
            QuietLabel {
                visible: root.record.hasBackground === true
                Layout.fillWidth: true
                text: root.record.runtimeStatus === "starting" ? qsTr("后台启动中") : root.record.runtimeStatus === "stopping" ? qsTr("后台停止中")
                    : root.record.backgroundRunning ? qsTr("后台运行中 · 随小叽恢复") : qsTr("后台未运行")
                color: root.record.backgroundRunning ? Theme.accent : Theme.muted
            }
        }
        RowLayout {
            Layout.fillWidth: false
            spacing: 6
            QuietLabel { visible: root.record.pending === true; text: qsTr("待重启"); color: Theme.warning }
            ActionButton { visible: root.record.hasPage === true && root.record.enabled === true; text: qsTr("打开"); quiet: true; onClicked: root.action("open", root.record.id, null) }
            ActionButton { visible: root.record.updateAvailable === true; text: qsTr("更新"); enabled: !root.busy; quiet: true; accented: true; onClicked: root.action("update", root.record.id, null) }
            ActionButton { text: qsTr("详情"); quiet: true; onClicked: root.action("details", root.record.id, root.record) }
            QuietSwitch {
                visible: !root.discovery
                checked: root.record.enabled === true
                enabled: !root.busy && (!root.record.blocked || checked) && (checked ? root.record.canDisable !== false : root.record.canEnable !== false)
                onClicked: root.action("enable", root.record.id, checked)
                Accessible.name: checked ? qsTr("停用 %1").arg(root.record.name) : qsTr("启用 %1").arg(root.record.name)
            }
            ActionButton {
                visible: root.discovery
                text: root.record.installed ? qsTr("已安装") : qsTr("安装")
                enabled: !root.busy && root.record.compatible === true && !root.record.installed
                onClicked: root.action("install", root.record.id, null)
            }
        }
    }
    Rectangle { anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom; height: 1; color: Theme.divider }
}
