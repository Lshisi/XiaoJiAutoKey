import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import XiaoJi.UI

QuietScrollView {
    id: root
    autoHideScrollBar: true
    readonly property QtObject page: settingsPageModel
    readonly property var guideTargets: ({ scope: scopeSwitch })
    contentWidth: availableWidth
    contentHeight: Math.max(pageColumn.implicitHeight, availableHeight)
    ButtonGroup { id: startupGroup }
    ButtonGroup { id: closeGroup }
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
                QuietHeading { text: qsTr("全局设置"); pageTitle: true }
                Rectangle {
                    Layout.fillWidth: true
                    Layout.topMargin: 6
                    Layout.bottomMargin: 6
                    radius: 8
                    color: Theme.surface
                    implicitHeight: previewContent.implicitHeight + 12
                    RowLayout {
                        id: previewContent
                        anchors.fill: parent
                        anchors.leftMargin: 12
                        anchors.rightMargin: 12
                        anchors.topMargin: 6
                        anchors.bottomMargin: 6
                        spacing: 10
                        Item {
                            Layout.preferredWidth: 36
                            Layout.preferredHeight: 36
                            FactionSymbol {
                                anchors.fill: parent
                                iconSource: Theme.colors.factionIcon
                                tintColor: Theme.text
                            }
                        }
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 3
                            QuietLabel { text: qsTr("门派主题"); color: Theme.muted }
                            QuietLabel { text: Theme.colors.factionName; font.weight: Theme.headingWeight }
                            Rectangle { Layout.fillWidth: true; implicitHeight: 3; radius: 1.5; color: Theme.accent }
                        }
                        FactionPicker { Layout.preferredWidth: 120 }
                    }
                }
                Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Theme.divider }
                QuietHeading { text: qsTr("生效范围") }
                RowLayout {
                    Layout.fillWidth: true
                    QuietLabel { text: qsTr("仅游戏客户端前台时生效"); Layout.fillWidth: true }
                    QuietSwitch { id: scopeSwitch; checked: root.page.state.gameGateEnabled; onClicked: root.page.gameGateEnabledChanged(checked); Accessible.name: qsTr("仅游戏客户端前台时生效") }
                }
                RowLayout {
                    Layout.fillWidth: true
                    ActionButton { text: qsTr("管理进程…"); leadingIcon: "monitor"; onClicked: root.page.gameGateProcessesEditRequested() }
                    QuietLabel {
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        text: root.page.state.gameGateProcessCount > 0
                            ? qsTr("剑网3及 %1 个自定义程序").arg(root.page.state.gameGateProcessCount) : qsTr("剑网3已内置")
                        color: Theme.muted
                        elide: Text.ElideRight
                        wrapMode: Text.NoWrap
                    }
                }
                Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Theme.divider }
                QuietHeading { text: qsTr("开机自启") }
                RowLayout {
                    Layout.fillWidth: true
                    QuietLabel { text: qsTr("登录 Windows 后自动运行小叽"); Layout.fillWidth: true }
                    QuietSwitch { checked: root.page.state.autoStartEnabled; onClicked: root.page.autoStartToggled(checked); Accessible.name: qsTr("开机自启") }
                }
                Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Theme.divider }
                QuietHeading { text: qsTr("启动时") }
                QuietRadio { text: qsTr("进入系统托盘"); ButtonGroup.group: startupGroup; checked: root.page.state.startupAction === 0; onClicked: root.page.selectStartupAction(0) }
                QuietRadio { text: qsTr("显示主窗口"); ButtonGroup.group: startupGroup; checked: root.page.state.startupAction === 1; onClicked: root.page.selectStartupAction(1) }
                Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Theme.divider }
                QuietHeading { text: qsTr("关闭窗口时") }
                QuietRadio { text: qsTr("每次询问"); ButtonGroup.group: closeGroup; checked: root.page.state.closeAction === 0; onClicked: root.page.selectCloseAction(0) }
                QuietRadio { text: qsTr("最小化到系统托盘"); ButtonGroup.group: closeGroup; checked: root.page.state.closeAction === 1; onClicked: root.page.selectCloseAction(1) }
                QuietRadio { text: qsTr("直接退出程序"); ButtonGroup.group: closeGroup; checked: root.page.state.closeAction === 2; onClicked: root.page.selectCloseAction(2) }
                Item { Layout.fillHeight: true }
            }
        }
    }
}
