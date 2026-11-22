import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import XiaoJi.UI

QuietScrollView {
    id: root
    autoHideScrollBar: true
    readonly property QtObject page: driverPageModel
    readonly property bool busy: page.state.busy || page.state.pluginBusy
    readonly property var guideTargets: ({ driver: driverChoice })
    contentWidth: availableWidth
    contentHeight: Math.max(pageColumn.implicitHeight, availableHeight)
    ColumnLayout {
        id: pageColumn
        width: root.availableWidth
        height: Math.max(implicitHeight, root.availableHeight)
        QuietCard {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.leftMargin: Theme.pageMargin
            Layout.rightMargin: Theme.pageMargin
            Layout.topMargin: Theme.cardGap
            Layout.bottomMargin: Theme.cardGap
            implicitHeight: body.implicitHeight + topPadding + bottomPadding
            ColumnLayout {
                id: body
                anchors.fill: parent
                spacing: 10
                RowLayout {
                    Layout.fillWidth: true
                    QuietHeading { text: qsTr("驱动管理"); pageTitle: true; Layout.fillWidth: true }
                    ActionButton { text: qsTr("管理驱动插件"); onClicked: root.page.openPluginCenter() }
                }
                QuietLabel { Layout.fillWidth: true; text: qsTr("从已启用的驱动插件中选择，状态和管理操作随当前插件显示。"); color: Theme.muted }
                Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Theme.divider }
                QuietHeading { text: qsTr("运行模式") }
                RowLayout {
                    Layout.fillWidth: true
                    QuietLabel { text: qsTr("使用驱动"); Layout.fillWidth: true }
                    QuietCombo {
                        id: driverChoice
                        Layout.preferredWidth: 190
                        model: root.page.state.providers
                        textRole: "name"
                        valueRole: "id"
                        displayText: currentIndex >= 0 ? currentText : count > 0 ? qsTr("请选择驱动") : qsTr("暂无可用驱动")
                        currentIndex: root.page.state.providerIndex
                        enabled: !root.busy
                        onActivated: root.page.selectProvider(currentValue)
                        Accessible.name: qsTr("使用驱动")
                    }
                    ActionButton {
                        visible: root.page.state.installAvailable
                        text: root.page.state.installText
                        enabled: !root.busy
                        onClicked: root.page.selectProvider(root.page.state.providerId)
                    }
                    ActionButton { visible: root.page.state.pluginManagement; text: qsTr("安装 / 修复"); enabled: !root.busy; onClicked: root.page.managePlugin("repair") }
                }
                Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Theme.divider }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    QuietHeading { text: qsTr("当前状态"); Layout.fillWidth: true }
                    ActionButton { visible: root.page.state.diagnosticsAvailable; text: qsTr("诊断信息"); enabled: !root.busy; onClicked: root.page.diagnosticsRequested() }
                    ActionButton { visible: root.page.state.selfCheckAvailable; text: qsTr("一键自检"); enabled: !root.busy; onClicked: root.page.runSelfCheck() }
                }
                Flow {
                    Layout.fillWidth: true
                    spacing: 8
                    visible: root.page.state.hidInstalled || root.page.state.systemDriverUninstall
                    ActionButton { visible: root.page.state.systemDriverUninstall; text: qsTr("卸载所选系统驱动"); enabled: !root.busy; onClicked: root.page.managePlugin("uninstall") }
                    ActionButton { visible: root.page.state.hidInstalled; text: qsTr("清理旧 HID 残留"); enabled: !root.busy; onClicked: root.page.uninstallRetiredHid() }
                }
                RowLayout {
                    Layout.fillWidth: true
                    QuietLabel { text: qsTr("概要") }
                    QuietLabel { Layout.fillWidth: true; text: root.page.state.summary; font.weight: Theme.headingWeight; horizontalAlignment: Text.AlignRight }
                }
                QuietTextArea {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.minimumHeight: 210
                    autoHideScrollBar: true
                    text: root.page.state.details
                    readOnly: true
                    background: Rectangle {
                        radius: Theme.inputRadius
                        color: Theme.field
                        border.color: Theme.accentBorder
                    }
                    Accessible.name: qsTr("驱动详细状态")
                }
            }
        }
    }
}
