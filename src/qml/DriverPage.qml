import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import XiaoJi.UI

ScrollView {
    id: root
    readonly property QtObject page: driverPageModel
    contentWidth: availableWidth
    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
    ScrollBar.vertical: QuietScrollBar {}
    ColumnLayout {
        width: root.availableWidth
        spacing: Theme.cardGap
        QuietCard {
            Layout.fillWidth: true
            Layout.margins: Theme.pageMargin
            implicitHeight: body.implicitHeight + topPadding + bottomPadding
            ColumnLayout {
                id: body
                anchors.fill: parent
                spacing: 16
                QuietHeading { text: qsTr("驱动管理"); pageTitle: true }
                QuietLabel { Layout.fillWidth: true; text: qsTr("管理 DD、HID 和 USB 驱动，查看连接状态。"); color: Theme.muted }
                Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Theme.divider }
                RowLayout {
                    Layout.fillWidth: true
                    QuietHeading { text: qsTr("运行模式") }
                    Item { Layout.fillWidth: true }
                    QuietCombo {
                        Layout.preferredWidth: 180
                        model: ["DD", "HID", "USB"]
                        currentIndex: root.page.state.mode
                        enabled: !root.page.state.busy
                        onActivated: root.page.selectMode(currentIndex)
                        Accessible.name: qsTr("使用驱动")
                    }
                }
                Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Theme.divider }
                RowLayout {
                    Layout.fillWidth: true
                    QuietHeading { text: qsTr("当前状态") }
                    Item { Layout.fillWidth: true }
                    ActionButton { text: qsTr("刷新"); enabled: !root.page.state.busy; onClicked: root.page.refreshRequested() }
                    ActionButton { text: qsTr("驱动自检"); enabled: !root.page.state.busy; onClicked: root.page.selfCheckRequested() }
                }
                QuietLabel { Layout.fillWidth: true; text: root.page.state.summary }
                TextArea {
                    Layout.fillWidth: true
                    implicitHeight: contentHeight + topPadding + bottomPadding
                    text: root.page.state.details
                    readOnly: true
                    selectByMouse: true
                    wrapMode: TextEdit.Wrap
                    color: Theme.muted
                    selectionColor: Theme.accent
                    selectedTextColor: Theme.accentForeground
                    font.family: Theme.fontFamily
                    font.pixelSize: 12
                    background: Item {}
                    Accessible.name: qsTr("驱动详细状态")
                }
                Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Theme.divider }
                QuietHeading { text: qsTr("卸载驱动") }
                QuietLabel { Layout.fillWidth: true; text: qsTr("卸载前会停止引擎并释放按键，部分操作需要重启。"); color: Theme.muted; font.pixelSize: 12 }
                RowLayout {
                    ActionButton { text: qsTr("卸载 DD"); enabled: !root.page.state.busy && root.page.state.ddInstalled; onClicked: root.page.uninstallMode(0) }
                    ActionButton { text: qsTr("卸载 HID"); enabled: !root.page.state.busy && root.page.state.hidInstalled; onClicked: root.page.uninstallMode(1) }
                }
            }
        }
    }
}
