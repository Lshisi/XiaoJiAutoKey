import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import XiaoJi.UI

Rectangle {
    id: shell
    color: Theme.surfaceTint
    radius: 10
    readonly property Item workspaceDialogAnchor: {
        const page = pageLoader.status === Loader.Ready ? pageLoader.item : null
        return page && page.dialogAnchor !== undefined ? page.dialogAnchor : null
    }
    ColumnLayout {
        anchors.fill: parent
        spacing: 0
        TitleBar { id: shellTitleBar; Layout.fillWidth: true }
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0
            Rectangle {
                Layout.preferredWidth: Theme.sidebarWidth
                Layout.minimumWidth: Theme.sidebarWidth
                Layout.maximumWidth: Theme.sidebarWidth
                Layout.fillHeight: true
                color: Theme.sidebarBackground
                Rectangle { anchors.right: parent.right; width: 1; height: parent.height; color: Theme.border }
                ColumnLayout {
                    anchors.fill: parent
                    anchors.topMargin: 12
                    anchors.bottomMargin: 12
                    spacing: 2
                    FactionPicker { Layout.fillWidth: true; Layout.preferredHeight: 58; Layout.bottomMargin: 6; sidebar: true }
                    SidebarNavigation {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                    }
                }
            }
            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: Theme.surfaceTint
                Loader {
                    id: pageLoader
                    anchors.fill: parent
                    source: {
                        switch (appView.currentPage) {
                        case "keys": return "KeyPage.qml"
                        case "macro": return "MacroPage.qml"
                        case "usage": return "UsagePage.qml"
                        case "driver": return "DriverPage.qml"
                        case "plugins": return "PluginCenterPage.qml"
                        case "settings": return "SettingsPage.qml"
                        case "about": return "AboutPage.qml"
                        }
                        return appView.currentPage.indexOf("plugin:") === 0 ? "PluginPage.qml" : ""
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
        active: settingsPageModel.state.crispyThemeReceiptVisible
        sourceComponent: CrispyThemeReceipt {}
    }
    Loader {
        anchors.right: parent.right
        anchors.rightMargin: Theme.pageMargin
        anchors.bottom: parent.bottom
        anchors.bottomMargin: statusBar.height + 8
        width: Math.min(360, shell.width - 2 * Theme.pageMargin)
        active: shareBannerModel.visible && !onboardingModel.state.active
        visible: active
        sourceComponent: ShareImportBanner {}
    }
    Loader {
        anchors.fill: parent
        anchors.topMargin: shellTitleBar.height
        anchors.bottomMargin: statusBar.height
        active: onboardingModel.state.active
        visible: onboardingModel.state.visible
        sourceComponent: OnboardingOverlay {
            pageRoot: pageLoader.status === Loader.Ready ? pageLoader.item : null
        }
    }
}
