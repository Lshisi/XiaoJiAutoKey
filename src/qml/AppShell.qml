import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import XiaoJi.UI

Rectangle {
    id: shell
    color: Theme.surfaceTint
    radius: 10
    ColumnLayout {
        anchors.fill: parent
        spacing: 0
        TitleBar { Layout.fillWidth: true }
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0
            Rectangle {
                Layout.preferredWidth: Theme.sidebarWidth
                Layout.minimumWidth: Theme.sidebarWidth
                Layout.maximumWidth: Theme.sidebarWidth
                Layout.fillHeight: true
                color: Theme.surfaceTint
                Rectangle { anchors.right: parent.right; width: 1; height: parent.height; color: Theme.border }
                ColumnLayout {
                    anchors.fill: parent
                    anchors.topMargin: 12
                    anchors.bottomMargin: 12
                    spacing: 2
                    FactionPicker { Layout.fillWidth: true; Layout.preferredHeight: 58; Layout.bottomMargin: 6; sidebar: true }
                    Repeater {
                        model: [
                            { page: "keys", title: qsTr("按键连发") },
                            { page: "macro", title: qsTr("循环连发") },
                            { page: "driver", title: qsTr("驱动管理") },
                            { page: "settings", title: qsTr("全局设置") },
                            { page: "about", title: qsTr("关于软件") }
                        ]
                        delegate: ActionButton {
                            id: navButton
                            required property var modelData
                            readonly property bool selected: appView.currentPage === modelData.page
                            Layout.fillWidth: true
                            Layout.leftMargin: 6
                            Layout.rightMargin: 6
                            implicitHeight: Theme.navItemHeight
                            text: modelData.title
                            quiet: true
                            Accessible.name: modelData.title
                            onClicked: appView.selectPage(modelData.page)
                            contentItem: RowLayout {
                                spacing: 9
                                NavIcon { iconId: navButton.modelData.page; iconColor: navButton.selected ? Theme.accent : Theme.muted }
                                QuietLabel { text: navButton.text; Layout.fillWidth: true; color: Theme.text; elide: Text.ElideRight; wrapMode: Text.NoWrap }
                            }
                            background: Rectangle {
                                radius: Theme.buttonRadius
                                color: navButton.selected ? Theme.accentSoft : navButton.hovered ? Theme.hover : "transparent"
                                border.width: navButton.visualFocus ? 2 : 0
                                border.color: Theme.accent
                                Rectangle {
                                    visible: navButton.selected
                                    x: -2
                                    anchors.verticalCenter: parent.verticalCenter
                                    width: 3
                                    height: 16
                                    radius: 1.5
                                    color: Theme.accent
                                }
                            }
                        }
                    }
                    Item { Layout.fillHeight: true }
                }
            }
            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: Theme.surfaceTint
                Loader {
                    anchors.fill: parent
                    source: {
                        switch (appView.currentPage) {
                        case "keys": return "KeyPage.qml"
                        case "macro": return "MacroPage.qml"
                        case "driver": return "DriverPage.qml"
                        case "settings": return "SettingsPage.qml"
                        case "about": return "AboutPage.qml"
                        }
                        return ""
                    }
                }
            }
        }
        Rectangle {
            id: statusBar
            Layout.fillWidth: true
            implicitHeight: Theme.statusBarHeight
            color: Theme.surfaceTint
            radius: 10
            topLeftRadius: 0
            topRightRadius: 0
            Rectangle { width: parent.width; height: 1; color: Theme.divider }
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 14
                anchors.rightMargin: 14
                QuietLabel { text: qsTr("V%1").arg(appView.version); color: Theme.muted; font.pixelSize: 11 }
                Item { Layout.fillWidth: true }
                QuietLabel { text: qsTr("By：龙虎-闻成画"); color: Theme.muted; font.pixelSize: 11 }
            }
        }
    }
    Loader {
        anchors.right: parent.right
        anchors.rightMargin: Theme.pageMargin
        anchors.bottom: parent.bottom
        anchors.bottomMargin: statusBar.height + 8
        width: Math.min(360, shell.width - 2 * Theme.pageMargin)
        active: shareBannerModel.visible
        visible: active
        sourceComponent: ShareImportBanner {}
    }
}
