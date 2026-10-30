import QtQuick
import QtQuick.Controls
import QtQuick.Effects
import QtQuick.Layouts
import XiaoJi.UI

QuietScrollView {
    id: root
    autoHideScrollBar: true
    readonly property QtObject page: settingsPageModel
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
                            Image {
                                id: factionSymbol
                                anchors.fill: parent
                                source: Theme.colors.factionIcon
                                sourceSize: Qt.size(width * Screen.devicePixelRatio, height * Screen.devicePixelRatio)
                                fillMode: Image.PreserveAspectFit
                                visible: !Theme.dark
                            }
                            MultiEffect {
                                anchors.fill: parent
                                source: factionSymbol
                                colorization: 1
                                colorizationColor: Theme.text
                                visible: Theme.dark && factionSymbol.status === Image.Ready
                            }
                        }
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 3
                            QuietLabel { text: qsTr("门派主题"); color: Theme.muted }
                            QuietLabel { text: Theme.colors.factionName; font.weight: Font.Bold }
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
                    QuietSwitch { checked: root.page.state.gameGateEnabled; onClicked: root.page.gameGateEnabledChanged(checked); Accessible.name: qsTr("仅游戏客户端前台时生效") }
                }
                ActionButton { text: qsTr("管理游戏进程…"); onClicked: root.page.gameGateProcessesEditRequested() }
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
