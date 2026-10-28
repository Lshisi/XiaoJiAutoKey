import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import XiaoJi.UI

ScrollView {
    id: root
    readonly property QtObject page: settingsPageModel
    contentWidth: availableWidth
    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
    ScrollBar.vertical: QuietScrollBar {}
    ColumnLayout {
        width: root.availableWidth
        QuietCard {
            Layout.fillWidth: true
            Layout.margins: Theme.pageMargin
            implicitHeight: body.implicitHeight + topPadding + bottomPadding
            ColumnLayout {
                id: body
                anchors.fill: parent
                spacing: 16
                QuietHeading { text: qsTr("全局设置"); pageTitle: true }
                RowLayout {
                    Layout.fillWidth: true
                    QuietLabel { text: qsTr("门派主题"); Layout.fillWidth: true }
                    FactionPicker { Layout.preferredWidth: 180 }
                }
                Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Theme.divider }
                QuietHeading { text: qsTr("启动与关闭") }
                RowLayout {
                    Layout.fillWidth: true
                    QuietLabel { text: qsTr("启动时"); Layout.fillWidth: true }
                    QuietCombo {
                        Layout.preferredWidth: 180
                        model: [qsTr("驻留托盘"), qsTr("显示主窗口")]
                        currentIndex: root.page.state.startupAction
                        onActivated: root.page.selectStartupAction(currentIndex)
                        Accessible.name: qsTr("启动行为")
                    }
                }
                RowLayout {
                    Layout.fillWidth: true
                    QuietLabel { text: qsTr("关闭窗口时"); Layout.fillWidth: true }
                    QuietCombo {
                        Layout.preferredWidth: 180
                        model: [qsTr("每次询问"), qsTr("最小化到托盘"), qsTr("退出软件")]
                        currentIndex: root.page.state.closeAction
                        onActivated: root.page.selectCloseAction(currentIndex)
                        Accessible.name: qsTr("关闭行为")
                    }
                }
                QuietCheck { text: qsTr("开机自启"); checked: root.page.state.autoStartEnabled; onClicked: root.page.autoStartToggled(checked) }
                Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Theme.divider }
                QuietHeading { text: qsTr("生效范围") }
                QuietCheck { text: qsTr("仅在游戏窗口位于前台时生效"); checked: root.page.state.gameGateEnabled; onClicked: root.page.gameGateEnabledChanged(checked) }
                QuietLabel { Layout.fillWidth: true; text: qsTr("离开游戏时自动暂停并释放按键，返回后恢复。"); color: Theme.muted; font.pixelSize: 12 }
                ActionButton { text: qsTr("管理游戏进程"); onClicked: root.page.gameGateProcessesEditRequested() }
            }
        }
    }
}
