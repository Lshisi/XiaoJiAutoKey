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
                    color: Theme.crispyThemeEnabled ? Theme.sidebarBackground : Theme.surface
                    implicitHeight: previewContent.implicitHeight + 12
                    RowLayout {
                        id: previewContent
                        anchors.fill: parent
                        anchors.leftMargin: 12
                        anchors.rightMargin: 12
                        anchors.topMargin: 6
                        anchors.bottomMargin: 6
                        spacing: 10
                        ActionButton {
                            id: factionEmblem
                            Layout.preferredWidth: 36
                            Layout.preferredHeight: 36
                            quiet: true
                            leftPadding: 0
                            rightPadding: 0
                            topPadding: 0
                            bottomPadding: 0
                            enabled: Theme.colors.factionId === "cangjian"
                                && (!root.page.state.crispyThemeUnlocked || root.page.state.crispyThemeEnabled)
                            hint: root.page.state.crispyThemeEnabled ? qsTr("查看出餐单")
                                : !root.page.state.crispyThemeUnlocked && root.page.state.thursday
                                    ? qsTr("今天庄里的小叽，好像有点紧张。") : qsTr("藏剑")
                            onClicked: {
                                root.page.activateFactionEmblem()
                                if (Theme.motionEnabled && root.page.state.thursday && !root.page.state.crispyThemeUnlocked)
                                    emblemWiggle.restart()
                            }
                            contentItem: FactionSymbol {
                                id: emblemSymbol
                                iconSource: Theme.colors.factionIcon
                                tintColor: Theme.text
                                preserveColors: Theme.colors.crispyThemeEnabled
                            }
                            SequentialAnimation {
                                id: emblemWiggle
                                paused: !root.visible || !Theme.motionEnabled
                                RotationAnimator { target: emblemSymbol; from: 0; to: -8; duration: 60 }
                                RotationAnimator { target: emblemSymbol; from: -8; to: 6; duration: 90 }
                                RotationAnimator { target: emblemSymbol; from: 6; to: 0; duration: 60 }
                            }
                        }
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 3
                            RowLayout {
                                id: themeCaption
                                Layout.fillWidth: true
                                spacing: 5
                                QuietLabel { id: themeCaptionText; text: qsTr("门派主题"); color: Theme.muted }
                                Loader {
                                    active: Theme.crispyThemeEnabled && themeCaption.width >= themeCaptionText.implicitWidth + 23
                                    visible: active
                                    Layout.preferredWidth: 13
                                    Layout.preferredHeight: 13
                                    sourceComponent: QuietIcon { name: "drumstick"; pixelSize: 13; color: Theme.crispyGold; preserveColors: true }
                                }
                                Item { Layout.fillWidth: true }
                            }
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 8
                                QuietLabel {
                                    Layout.minimumWidth: 0
                                    text: Theme.colors.themeName
                                    font.weight: Theme.headingWeight
                                    elide: Text.ElideRight
                                    wrapMode: Text.NoWrap
                                }
                                ActionButton {
                                    visible: root.page.state.crispyThemeEnabled
                                    text: qsTr("查看出餐单")
                                    compact: true
                                    quiet: true
                                    onClicked: root.page.showCrispyThemeReceipt()
                                }
                                QuietLabel {
                                    visible: root.page.state.crispyThemeEnabled && root.page.state.thursday
                                    text: qsTr("今日出餐")
                                    color: Theme.accent
                                    font.pixelSize: Theme.captionFontSize
                                }
                                Item { Layout.fillWidth: true }
                            }
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
                QuietHeading { text: qsTr("软件与插件更新") }
                RowLayout {
                    Layout.fillWidth: true
                    QuietLabel { text: qsTr("后台检查更新"); Layout.fillWidth: true }
                    QuietSwitch { checked: root.page.state.automaticUpdateChecks; onClicked: root.page.automaticUpdateChecksToggled(checked); Accessible.name: qsTr("后台检查更新") }
                }
                QuietLabel { Layout.fillWidth: true; text: qsTr("有更新时提示，由你选择下载、安装和重启。"); color: Theme.muted }
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
