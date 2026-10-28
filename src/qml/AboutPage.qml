import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import XiaoJi.UI

ScrollView {
    id: root
    readonly property QtObject page: aboutPageModel
    contentWidth: availableWidth
    contentHeight: Math.max(pageColumn.implicitHeight, root.availableHeight)
    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
    ScrollBar.vertical: QuietScrollBar {}
    ColumnLayout {
        id: pageColumn
        width: root.availableWidth
        height: Math.max(pageColumn.implicitHeight, root.availableHeight)
        QuietCard {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.margins: Theme.pageMargin
            implicitHeight: body.implicitHeight + topPadding + bottomPadding
            ColumnLayout {
                id: body
                anchors.fill: parent
                spacing: 18
                QuietHeading { text: qsTr("关于软件"); pageTitle: true }
                RowLayout {
                    spacing: 14
                    Image { Layout.preferredWidth: 44; Layout.preferredHeight: 44; source: "qrc:/assets/icon.png"; sourceSize: Qt.size(88, 88); fillMode: Image.PreserveAspectFit; Accessible.ignored: true }
                    ColumnLayout {
                        QuietHeading { text: qsTr("小叽按键") }
                        QuietLabel { text: qsTr("一个轻量、高效的按键自动化工具"); color: Theme.muted }
                    }
                }
                Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Theme.divider }
                QuietHeading { text: qsTr("软件更新") }
                RowLayout {
                    Layout.fillWidth: true
                    QuietLabel { text: qsTr("当前版本：V%1").arg(appView.version); Layout.fillWidth: true }
                    ActionButton {
                        text: root.page.state.buttonText
                        enabled: root.page.state.canUpdate
                        primary: root.page.state.updateState === "available" || root.page.state.updateState === "staged"
                        onClicked: root.page.activateUpdate()
                    }
                }
                QuietLabel {
                    Layout.fillWidth: true
                    visible: root.page.state.latestVersion.length > 0
                    text: qsTr("发现新版本 V%1").arg(root.page.state.latestVersion)
                    color: Theme.accent
                }
                Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Theme.divider }
                RowLayout {
                    Layout.fillWidth: true
                    QuietHeading { text: qsTr("配置说明"); Layout.fillWidth: true }
                    ActionButton { text: qsTr("打开使用说明"); onClicked: root.page.usageGuideRequested() }
                    ActionButton { text: qsTr("开源许可"); onClicked: root.page.licenseRequested() }
                }
                Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Theme.divider }
                QuietHeading { text: qsTr("免责声明") }
                QuietLabel {
                    Layout.fillWidth: true
                    color: Theme.muted
                    text: qsTr("1. 本软件仅供学习和交流使用\n2. 使用本软件产生的一切后果由用户自行承担\n3. 作者不对软件使用造成的任何损失负责\n4. 禁止使用本软件进行任何违法活动")
                }
            }
        }
    }
}
