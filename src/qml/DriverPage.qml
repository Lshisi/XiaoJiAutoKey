import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import XiaoJi.UI

QuietScrollView {
    id: root
    autoHideScrollBar: true
    readonly property QtObject page: driverPageModel
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
                QuietHeading { text: qsTr("驱动管理"); pageTitle: true }
                QuietLabel { Layout.fillWidth: true; text: qsTr("管理 DD、HID 和 USB 小叽硬件驱动，查看当前状态、切换驱动、安装或卸载驱动。"); color: Theme.muted }
                Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Theme.divider }
                QuietHeading { text: qsTr("运行模式") }
                RowLayout {
                    Layout.fillWidth: true
                    QuietLabel { text: qsTr("使用驱动"); Layout.fillWidth: true }
                    QuietCombo {
                        id: driverChoice
                        Layout.preferredWidth: 190
                        model: ["DD", "HID", "USB"]
                        currentIndex: root.page.state.mode
                        enabled: !root.page.state.busy
                        onActivated: root.page.selectMode(currentIndex)
                        Accessible.name: qsTr("使用驱动")
                    }
                    ActionButton {
                        visible: root.page.state.installAvailable
                        text: root.page.state.installText
                        enabled: !root.page.state.busy
                        onClicked: root.page.selectMode(root.page.state.mode)
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
