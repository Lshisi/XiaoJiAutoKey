import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import XiaoJi.UI

ScrollView {
    id: root
    readonly property QtObject page: driverPageModel
    contentWidth: availableWidth
    contentHeight: Math.max(pageColumn.implicitHeight, availableHeight)
    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
    ScrollBar.vertical: QuietScrollBar {}
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
                QuietHeading { text: qsTr("驱动管理"); pageTitle: true }
                QuietLabel { Layout.fillWidth: true; text: qsTr("管理 DD、HID 和 USB 小叽硬件驱动，查看当前状态、切换驱动、安装或卸载驱动。"); color: Theme.muted }
                Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Theme.divider }
                QuietHeading { text: qsTr("运行模式") }
                RowLayout {
                    Layout.fillWidth: true
                    QuietLabel { text: qsTr("使用驱动"); Layout.fillWidth: true }
                    QuietCombo {
                        Layout.preferredWidth: 190
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
                    spacing: 8
                    QuietHeading { text: qsTr("当前状态"); Layout.fillWidth: true }
                    ActionButton { text: qsTr("一键自检"); primary: true; enabled: !root.page.state.busy; onClicked: root.page.selfCheckRequested() }
                    ActionButton { visible: root.page.state.ddInstalled; primary: true; text: qsTr("卸载 DD"); enabled: !root.page.state.busy; onClicked: root.page.uninstallMode(0) }
                    ActionButton { visible: root.page.state.hidInstalled; primary: true; text: qsTr("卸载 HID"); enabled: !root.page.state.busy; onClicked: root.page.uninstallMode(1) }
                }
                RowLayout {
                    Layout.fillWidth: true
                    QuietLabel { text: qsTr("概要") }
                    QuietLabel { Layout.fillWidth: true; text: root.page.state.summary; font.weight: Font.Bold; horizontalAlignment: Text.AlignRight }
                }
                TextArea {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.minimumHeight: 210
                    text: root.page.state.details
                    readOnly: true
                    selectByMouse: true
                    wrapMode: TextEdit.Wrap
                    color: Theme.muted
                    font.family: Theme.fontFamily
                    font.pixelSize: 12
                    padding: 10
                    background: Rectangle {
                        radius: Theme.inputRadius
                        color: Theme.accentSubtle
                        border.color: Theme.accentBorder
                    }
                    ScrollBar.vertical: QuietScrollBar {}
                    Accessible.name: qsTr("驱动详细状态")
                }
            }
        }
    }
}
