import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import XiaoJi.UI

Item {
    id: workspace
    required property QtObject page
    property bool macroMode: false
    readonly property var guideTargets: ({
        profile: profileToolbar, add: addButton, mode: keyTable.guideRow,
        hotkey: hotkeyPanel, run: runPanel, pause: shortcutBar,
        options: optionsLoader.status === Loader.Ready ? optionsLoader.item : null
    })
    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Theme.pageMargin
        anchors.rightMargin: Theme.pageMargin
        anchors.topMargin: Theme.cardGap
        anchors.bottomMargin: Theme.cardGap
        spacing: Theme.cardGap
        QuietCard {
            id: listCard
            objectName: "workspaceDialogAnchor"
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredWidth: 0
            Layout.minimumWidth: 0
            ColumnLayout {
                anchors.fill: parent
                spacing: 10
                ProfileToolbar { id: profileToolbar; Layout.fillWidth: true; page: workspace.page; macroMode: workspace.macroMode }
                ProfileNoteSummary { Layout.fillWidth: true; Layout.minimumWidth: 0; Layout.preferredWidth: 0; page: workspace.page }
                RowLayout {
                    Layout.fillWidth: true
                    Layout.topMargin: 8
                    QuietHeading { text: workspace.macroMode ? qsTr("循环列表") : qsTr("按键列表"); pageTitle: true }
                    ActionButton {
                        visible: !workspace.macroMode && keyPageModel.state.updateVersion.length > 0
                        text: qsTr("有新版本 V%1").arg(keyPageModel.state.updateVersion)
                        quiet: true
                        accented: true
                        onClicked: keyPageModel.updateBadgeClicked()
                    }
                    Item { Layout.fillWidth: true }
                    ActionButton {
                        id: addButton
                        text: qsTr("添加按键")
                        leadingIcon: "add"
                        compact: true
                        enabled: !workspace.page.state.running
                        onClicked: workspace.macroMode ? workspace.page.addStepRequested() : workspace.page.addRequested()
                    }
                }
                WorkspaceTable {
                    id: keyTable
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.leftMargin: 1 - listCard.leftPadding
                    Layout.rightMargin: 1 - listCard.rightPadding
                    page: workspace.page
                    macroMode: workspace.macroMode
                    contentLeftPadding: Math.max(0, listCard.leftPadding - 1)
                    contentRightPadding: Math.max(0, listCard.rightPadding - 1)
                }
                ShortcutBar { id: shortcutBar; Layout.fillWidth: true; page: workspace.page; macroMode: workspace.macroMode }
            }
        }
        ColumnLayout {
            Layout.fillWidth: false
            Layout.preferredWidth: Theme.controlPanelWidth
            Layout.minimumWidth: Theme.controlPanelWidth
            Layout.maximumWidth: Theme.controlPanelWidth
            Layout.fillHeight: true
            spacing: Theme.cardGap
            RunPanel { id: runPanel; Layout.fillWidth: true; Layout.fillHeight: false; Layout.minimumHeight: implicitHeight; page: workspace.page }
            HotkeyPanel { id: hotkeyPanel; Layout.fillWidth: true; Layout.fillHeight: false; Layout.minimumHeight: implicitHeight; page: workspace.page }
            Loader {
                id: optionsLoader
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumHeight: implicitHeight
                sourceComponent: workspace.macroMode ? macroOptions : repeatOptions
                Component { id: repeatOptions; RepeatOptions { page: workspace.page } }
                Component { id: macroOptions; MacroOptions { page: workspace.page } }
            }
        }
    }
}
