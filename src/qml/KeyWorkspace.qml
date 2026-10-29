import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import XiaoJi.UI

Item {
    id: workspace
    required property QtObject page
    property bool macroMode: false
    readonly property var rows: macroMode ? page.state.steps : page.state.bindings
    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Theme.pageMargin
        anchors.rightMargin: Theme.pageMargin
        anchors.topMargin: Theme.cardGap
        anchors.bottomMargin: Theme.cardGap
        spacing: Theme.cardGap
        QuietCard {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredWidth: 0
            Layout.horizontalStretchFactor: 5
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
                        onClicked: keyPageModel.updateBadgeClicked()
                    }
                    Item { Layout.fillWidth: true }
                    ActionButton {
                        text: qsTr("+  添加按键")
                        primary: true
                        enabled: !workspace.page.state.running
                        onClicked: workspace.macroMode ? workspace.page.addStepRequested() : workspace.page.addRequested()
                    }
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: 0
                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: Theme.tableHeaderHeight
                        color: Theme.accentSubtle
                        RowLayout {
                            anchors.fill: parent
                            anchors.rightMargin: 8
                            spacing: 0
                            QuietLabel { Layout.preferredWidth: workspace.macroMode ? 32 : 42; text: workspace.macroMode ? "" : qsTr("状态"); color: Theme.muted }
                            QuietLabel { Layout.fillWidth: true; Layout.preferredWidth: 0; text: qsTr("按键"); color: Theme.muted }
                            QuietLabel { visible: !workspace.macroMode; Layout.fillWidth: true; Layout.preferredWidth: 0; text: qsTr("模式"); color: Theme.muted }
                            QuietLabel { visible: !workspace.macroMode; Layout.fillWidth: true; Layout.preferredWidth: 0; text: qsTr("优先"); horizontalAlignment: Text.AlignHCenter; color: Theme.muted }
                            QuietLabel { Layout.fillWidth: true; Layout.preferredWidth: 0; text: workspace.macroMode ? qsTr("间隔 (ms)") : qsTr("延迟"); color: Theme.muted }
                            QuietLabel { Layout.preferredWidth: 72; text: qsTr("操作"); color: Theme.muted }
                        }
                    }
                    ListView {
                        id: list
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        boundsBehavior: Flickable.StopAtBounds
                        model: workspace.rows
                        delegate: Loader {
                            id: rowLoader
                            required property var modelData
                            required property int index
                            width: list.width - 8
                            height: Theme.tableRowHeight
                            sourceComponent: workspace.macroMode ? stepDelegate : bindingDelegate
                            Component {
                                id: bindingDelegate
                                BindingRow { page: workspace.page; entry: rowLoader.modelData; enabled: !workspace.page.state.running }
                            }
                            Component {
                                id: stepDelegate
                                MacroStepRow { page: workspace.page; entry: rowLoader.modelData; rowIndex: rowLoader.index; rowCount: list.count; enabled: !workspace.page.state.running }
                            }
                        }
                        ScrollBar.vertical: QuietScrollBar {}
                    }
                }
                ShortcutBar { Layout.fillWidth: true; page: workspace.page; macroMode: workspace.macroMode }
            }
        }
        ScrollView {
            id: sideScroll
            Layout.fillWidth: true
            Layout.preferredWidth: 0
            Layout.horizontalStretchFactor: 3
            Layout.minimumWidth: 190
            Layout.fillHeight: true
            contentWidth: availableWidth
            contentHeight: Math.max(sideColumn.implicitHeight, availableHeight)
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
            ScrollBar.vertical: QuietScrollBar {}
            ColumnLayout {
                id: sideColumn
                width: sideScroll.availableWidth
                height: Math.max(implicitHeight, sideScroll.availableHeight)
                spacing: Theme.cardGap
                RunPanel { Layout.fillWidth: true; page: workspace.page }
                HotkeyPanel { Layout.fillWidth: true; page: workspace.page }
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
}
