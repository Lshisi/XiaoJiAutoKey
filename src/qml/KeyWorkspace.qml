import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import XiaoJi.UI

Item {
    id: workspace
    required property QtObject page
    property bool macroMode: false
    readonly property var rows: macroMode ? page.state.steps : page.state.bindings
    QtObject {
        id: tableColumns
        readonly property real rowWidth: Math.max(0, list.width)
        readonly property real contentLeftPadding: Math.max(0, listCard.leftPadding - 1)
        readonly property real contentRightPadding: Math.max(0, listCard.rightPadding - 1) + tableScrollBar.implicitWidth
        readonly property real contentWidth: Math.max(0, rowWidth - contentLeftPadding - contentRightPadding)
        readonly property int leadingWidth: workspace.macroMode ? Theme.iconSize : 42
        readonly property int actionPadding: 4
        readonly property int actionWidth: 2 * Theme.iconSize + 2 * actionPadding
        readonly property real fieldWidth: Math.max(0,
            (contentWidth - leadingWidth - actionWidth) / (workspace.macroMode ? 2 : 4))
    }
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
                ProfileToolbar { Layout.fillWidth: true; page: workspace.page; macroMode: workspace.macroMode }
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
                        text: qsTr("+  添加按键")
                        primary: true
                        compact: true
                        enabled: !workspace.page.state.running
                        onClicked: workspace.macroMode ? workspace.page.addStepRequested() : workspace.page.addRequested()
                    }
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: 0
                    Item {
                        Layout.fillWidth: true
                        implicitHeight: Theme.tableHeaderHeight
                        Rectangle {
                            anchors.fill: parent
                            anchors.leftMargin: 1 - listCard.leftPadding
                            anchors.rightMargin: 1 - listCard.rightPadding
                            color: Theme.accentSubtle
                            Accessible.ignored: true
                        }
                        RowLayout {
                            anchors.fill: parent
                            anchors.rightMargin: tableScrollBar.implicitWidth
                            spacing: 0
                            QuietLabel { Layout.preferredWidth: tableColumns.leadingWidth; text: workspace.macroMode ? "" : qsTr("状态"); horizontalAlignment: Text.AlignHCenter; color: Theme.muted }
                            QuietLabel { Layout.fillWidth: true; Layout.minimumWidth: 0; Layout.preferredWidth: tableColumns.fieldWidth; text: qsTr("按键"); horizontalAlignment: Text.AlignHCenter; color: Theme.muted }
                            QuietLabel { visible: !workspace.macroMode; Layout.fillWidth: true; Layout.minimumWidth: 0; Layout.preferredWidth: tableColumns.fieldWidth; text: qsTr("模式"); horizontalAlignment: Text.AlignHCenter; color: Theme.muted }
                            QuietLabel { visible: !workspace.macroMode; Layout.fillWidth: true; Layout.minimumWidth: 0; Layout.preferredWidth: tableColumns.fieldWidth; text: qsTr("优先"); horizontalAlignment: Text.AlignHCenter; color: Theme.muted }
                            QuietLabel { Layout.fillWidth: true; Layout.minimumWidth: 0; Layout.preferredWidth: tableColumns.fieldWidth; text: workspace.macroMode ? qsTr("间隔 (ms)") : qsTr("延迟"); horizontalAlignment: Text.AlignHCenter; color: Theme.muted }
                            QuietLabel { Layout.preferredWidth: tableColumns.actionWidth; text: qsTr("操作"); horizontalAlignment: Text.AlignHCenter; color: Theme.muted }
                        }
                    }
                    ListView {
                        id: list
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        Layout.leftMargin: 1 - listCard.leftPadding
                        Layout.rightMargin: 1 - listCard.rightPadding
                        clip: true
                        boundsBehavior: Flickable.StopAtBounds
                        model: workspace.rows
                        delegate: Loader {
                            id: rowLoader
                            required property var modelData
                            required property int index
                            width: tableColumns.rowWidth
                            height: Theme.tableRowHeight
                            sourceComponent: workspace.macroMode ? stepDelegate : bindingDelegate
                            Component {
                                id: bindingDelegate
                                BindingRow { page: workspace.page; entry: rowLoader.modelData; columns: tableColumns; enabled: !workspace.page.state.running }
                            }
                            Component {
                                id: stepDelegate
                                MacroStepRow { page: workspace.page; entry: rowLoader.modelData; columns: tableColumns; rowIndex: rowLoader.index; rowCount: list.count; enabled: !workspace.page.state.running }
                            }
                        }
                        ScrollBar.vertical: QuietScrollBar { id: tableScrollBar }
                    }
                }
                ShortcutBar { Layout.fillWidth: true; page: workspace.page; macroMode: workspace.macroMode }
            }
        }
        ColumnLayout {
            Layout.fillWidth: false
            Layout.preferredWidth: Theme.controlPanelWidth
            Layout.minimumWidth: Theme.controlPanelWidth
            Layout.maximumWidth: Theme.controlPanelWidth
            Layout.fillHeight: true
            spacing: Theme.cardGap
            RunPanel { Layout.fillWidth: true; Layout.fillHeight: false; Layout.minimumHeight: implicitHeight; page: workspace.page }
            HotkeyPanel { Layout.fillWidth: true; Layout.fillHeight: false; Layout.minimumHeight: implicitHeight; page: workspace.page }
            Loader {
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
