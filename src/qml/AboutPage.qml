import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import XiaoJi.UI

ScrollView {
    id: root
    readonly property QtObject page: aboutPageModel
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
                QuietHeading { text: qsTr("关于软件"); pageTitle: true }
                QuietCard {
                    Layout.fillWidth: true
                    implicitHeight: info.implicitHeight + topPadding + bottomPadding
                    RowLayout {
                        id: info
                        anchors.fill: parent
                        spacing: 12
                        Image { Layout.preferredWidth: 48; Layout.preferredHeight: 48; source: "qrc:/assets/icon.png"; sourceSize: Qt.size(48 * Screen.devicePixelRatio, 48 * Screen.devicePixelRatio); fillMode: Image.PreserveAspectFit; Accessible.ignored: true }
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2
                            QuietHeading { text: qsTr("小叽按键") }
                            QuietLabel { Layout.fillWidth: true; text: qsTr("一个轻量、高效的按键自动化工具"); color: Theme.muted }
                        }
                    }
                }
                QuietCard {
                    Layout.fillWidth: true
                    implicitHeight: update.implicitHeight + topPadding + bottomPadding
                    ColumnLayout {
                        id: update
                        anchors.fill: parent
                        spacing: 10
                        QuietHeading { text: qsTr("软件更新") }
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8
                            QuietLabel { text: qsTr("当前版本: V%1").arg(appView.version) }
                            QuietLabel { Layout.fillWidth: true; visible: root.page.state.latestVersion.length > 0; text: qsTr("发现新版本 V%1").arg(root.page.state.latestVersion); color: Theme.accent }
                            Item { Layout.fillWidth: true; visible: root.page.state.latestVersion.length === 0 }
                            ActionButton {
                                text: root.page.state.buttonText
                                enabled: root.page.state.canUpdate
                                primary: root.page.state.updateState === "available" || root.page.state.updateState === "staged"
                                onClicked: root.page.activateUpdate()
                            }
                        }
                    }
                }
                QuietCard {
                    Layout.fillWidth: true
                    implicitHeight: usage.implicitHeight + topPadding + bottomPadding
                    ColumnLayout {
                        id: usage
                        anchors.fill: parent
                        spacing: 10
                        QuietHeading { text: qsTr("配置说明") }
                        RowLayout {
                            Layout.fillWidth: true
                            QuietLabel { Layout.fillWidth: true; text: qsTr("小叽按键配置说明") }
                            ActionButton { text: qsTr("打开"); onClicked: root.page.usageGuideRequested() }
                        }
                    }
                }
                QuietCard {
                    Layout.fillWidth: true
                    implicitHeight: disclaimer.implicitHeight + topPadding + bottomPadding
                    ColumnLayout {
                        id: disclaimer
                        anchors.fill: parent
                        spacing: 10
                        QuietHeading { text: qsTr("免责声明") }
                        QuietLabel { Layout.fillWidth: true; text: qsTr("1. 本软件仅供学习和交流使用\n2. 使用本软件产生的一切后果由用户自行承担\n3. 作者不对软件使用造成的任何损失负责\n4. 禁止使用本软件进行任何违法活动"); color: Theme.muted }
                    }
                }
                Item { Layout.fillHeight: true }
            }
        }
    }
}
