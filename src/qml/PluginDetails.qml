import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import XiaoJi.UI

Popup {
    id: popup
    required property var record
    required property QtObject page
    property bool registered: false
    signal dismissed()
    width: Math.min(430, parent.width - 32)
    height: Math.min(content.implicitHeight + 2 * padding, parent.height - 32)
    x: (parent.width - width) / 2
    y: (parent.height - height) / 2
    padding: Theme.cardPadding
    modal: true
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    background: Rectangle { radius: Theme.dialogRadius; color: Theme.surface; border.color: Theme.border }
    onOpened: { registered = true; page.dispatch("dialog", "", true) }
    function releaseDialog() { if (registered) { registered = false; page.dispatch("dialog", "", false) } }
    onClosed: { releaseDialog(); dismissed() }
    Component.onDestruction: releaseDialog()
    function perform(operation, value) {
        const owner = page
        const pluginId = record.id
        close()
        owner.dispatch(operation, pluginId, value === undefined ? null : value)
    }
    contentItem: QuietScrollView {
        id: scroll
        contentWidth: availableWidth
        contentHeight: content.implicitHeight
        ColumnLayout {
            id: content
            width: scroll.availableWidth
            spacing: 12
            RowLayout {
                Layout.fillWidth: true
                QuietHeading { text: popup.record.name; Layout.fillWidth: true }
                IconButton { iconName: "close"; hint: qsTr("关闭插件详情"); onClicked: popup.close() }
            }
            QuietLabel { text: popup.record.description; Layout.fillWidth: true; color: Theme.muted }
            QuietLabel { text: qsTr("版本：%1").arg(popup.record.version || "—"); Layout.fillWidth: true }
            QuietLabel { text: qsTr("兼容小叽：%1 起%2").arg(popup.record.minHostVersion || "—").arg(popup.record.maxHostVersion ? qsTr("，至 %1").arg(popup.record.maxHostVersion) : ""); Layout.fillWidth: true }
            QuietLabel { text: qsTr("作者：%1").arg(popup.record.author); Layout.fillWidth: true }
            QuietLabel { text: popup.record.reviewed ? qsTr("审核：此版本审核通过") : qsTr("审核：未审核"); Layout.fillWidth: true }
            QuietLabel { text: qsTr("来源：%1").arg(popup.record.repository || popup.record.source); Layout.fillWidth: true }
            QuietLabel { visible: !!popup.record.installationSource; text: qsTr("安装来源：%1").arg(popup.record.installationSource || ""); Layout.fillWidth: true; color: Theme.muted }
            QuietLabel { text: qsTr("使用能力：%1").arg((popup.record.permissions || []).join("、")); Layout.fillWidth: true; color: Theme.muted }
            QuietLabel { visible: !!popup.record.error && popup.record.error.length > 0; text: popup.record.error || ""; color: Theme.danger; Layout.fillWidth: true }
            QuietLabel { visible: !!popup.record.disableReason; text: popup.record.disableReason || ""; color: Theme.muted; Layout.fillWidth: true }
            QuietLabel { visible: !!popup.record.removalError; text: popup.record.removalError || ""; color: Theme.warning; Layout.fillWidth: true }
            QuietLabel { visible: !!popup.record.runtimeError; text: popup.record.runtimeError || ""; color: Theme.danger; Layout.fillWidth: true }
            QuietLabel {
                visible: popup.record.hasBackground === true
                text: popup.record.backgroundRunning ? qsTr("后台运行中。关闭设置页后继续运行，下次启动小叽时恢复。")
                    : popup.record.runtimeStatus === "starting" ? qsTr("后台启动中…") : popup.record.runtimeStatus === "stopping" ? qsTr("后台停止中…")
                    : qsTr("后台未运行。首次安装后需主动启动，停止后不再随小叽恢复。")
                Layout.fillWidth: true
                color: Theme.muted
            }
            QuietLabel {
                visible: popup.record.updateAvailable === true
                text: qsTr("可更新至 %1 · %2").arg(popup.record.latestVersion || "—").arg(popup.record.latestReviewed ? qsTr("目标版本已审核") : qsTr("目标版本未审核"))
                color: Theme.accent
                Layout.fillWidth: true
            }
            QuietLabel { visible: !!popup.record.notes; text: popup.record.notes || ""; color: Theme.muted; Layout.fillWidth: true }
            Flow {
                Layout.fillWidth: true
                spacing: 8
                ActionButton { visible: popup.record.pending === true; text: qsTr("重启应用"); enabled: !popup.page.state.busy; onClicked: popup.perform("restart") }
                ActionButton { visible: popup.record.hasPage === true && popup.record.enabled === true; text: qsTr("打开页面"); onClicked: popup.perform("open") }
                ActionButton {
                    visible: popup.record.hasBackground === true && popup.record.enabled === true && popup.record.runtimeStatus !== undefined && !popup.record.blocked
                    text: popup.record.backgroundRunning ? qsTr("停止后台") : qsTr("启动后台")
                    enabled: !popup.page.state.busy && popup.record.runtimeStatus !== "starting" && popup.record.runtimeStatus !== "stopping"
                    onClicked: popup.perform("background", !popup.record.backgroundRunning)
                }
                ActionButton { visible: popup.record.updateAvailable === true; text: qsTr("下载更新"); enabled: !popup.page.state.busy; onClicked: popup.perform("update") }
                ActionButton { visible: popup.record.builtin === true; text: qsTr("恢复随包版本"); enabled: !popup.page.state.busy && popup.record.canRestoreBundled === true; onClicked: popup.perform("restore-bundled") }
                ActionButton { visible: popup.record.hasManagement === true && popup.record.enabled === true; text: qsTr("安装 / 修复系统驱动"); enabled: !popup.page.state.busy; onClicked: popup.perform("manage") }
                ActionButton { visible: popup.record.hasSystemDriverUninstall === true && popup.record.enabled === true; text: qsTr("卸载系统驱动"); enabled: !popup.page.state.busy; onClicked: popup.perform("uninstall-system") }
                ActionButton { visible: popup.record.enabled !== undefined && !popup.record.builtin; text: popup.record.removalPendingRestart ? qsTr("继续卸载") : popup.record.removalError ? qsTr("重试卸载") : qsTr("卸载插件"); enabled: !popup.page.state.busy && popup.record.canRemove !== false; onClicked: popup.perform("remove") }
            }
            Repeater {
                model: popup.record.enabled === true && !popup.record.blocked ? (popup.record.windows || []) : []
                delegate: RowLayout {
                    id: windowRow
                    required property var modelData
                    Layout.fillWidth: true
                    QuietLabel { text: windowRow.modelData.title; Layout.fillWidth: true; Layout.minimumWidth: 0; elide: Text.ElideRight }
                    ActionButton { text: windowRow.modelData.visible ? qsTr("显示") : qsTr("打开窗口"); enabled: !popup.page.state.busy; onClicked: popup.perform("window", windowRow.modelData.id) }
                    ActionButton { visible: windowRow.modelData.clickThrough === true; text: qsTr("恢复交互"); enabled: !popup.page.state.busy; onClicked: popup.perform("window-interactive", windowRow.modelData.id) }
                    ActionButton { visible: windowRow.modelData.open === true; text: qsTr("关闭"); enabled: !popup.page.state.busy; onClicked: popup.perform("window-close", windowRow.modelData.id) }
                }
            }
            Flow {
                Layout.fillWidth: true
                spacing: 8
                Repeater {
                    model: popup.record.enabled === true && !popup.record.blocked ? (popup.record.commands || []) : []
                    delegate: ActionButton {
                        required property var modelData
                        width: Math.min(implicitWidth, popup.availableWidth)
                        text: modelData.title
                        hint: modelData.title
                        enabled: !popup.page.state.busy && (modelData.target === "native" || popup.record.backgroundRunning === true)
                        onClicked: popup.perform("command", modelData.id)
                    }
                }
            }
        }
    }
}
