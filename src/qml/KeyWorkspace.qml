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
        anchors.margins: Theme.pageMargin
        spacing: Theme.cardGap
        QuietCard {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumWidth: 0
            ColumnLayout {
                anchors.fill: parent
                spacing: 14
                ProfileToolbar { Layout.fillWidth: true; page: workspace.page; macroMode: workspace.macroMode; enabled: !workspace.page.state.running }
                RowLayout {
                    Layout.fillWidth: true
                    Layout.topMargin: 4
                    QuietHeading { text: workspace.macroMode ? qsTr("循环步骤") : qsTr("按键列表"); pageTitle: true }
                    ActionButton {
                        visible: !workspace.macroMode && keyPageModel.state.updateVersion.length > 0
                        text: qsTr("有新版本")
                        quiet: true
                        onClicked: keyPageModel.updateBadgeClicked()
                    }
                    Item { Layout.fillWidth: true }
                    ActionButton {
                        text: workspace.macroMode ? qsTr("＋ 添加步骤") : qsTr("＋ 添加按键")
                        enabled: !workspace.page.state.running
                        onClicked: workspace.macroMode ? workspace.page.addStepRequested() : workspace.page.addRequested()
                    }
                }
                ListView {
                    id: list
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    boundsBehavior: Flickable.StopAtBounds
                    model: workspace.rows
                    headerPositioning: ListView.OverlayHeader
                    header: Rectangle {
                        width: list.width
                        height: 34
                        z: 3
                        color: Theme.background
                        RowLayout {
                            anchors.fill: parent
                            spacing: 0
                            QuietLabel { Layout.preferredWidth: 34; text: workspace.macroMode ? qsTr("排序") : qsTr("状态"); font.pixelSize: 11; color: Theme.muted }
                            QuietLabel { Layout.fillWidth: true; text: qsTr("按键"); font.pixelSize: 11; color: Theme.muted }
                            QuietLabel { visible: !workspace.macroMode; Layout.preferredWidth: 66; text: qsTr("模式"); font.pixelSize: 11; color: Theme.muted }
                            QuietLabel { visible: !workspace.macroMode; Layout.preferredWidth: 40; text: qsTr("优先"); font.pixelSize: 11; color: Theme.muted }
                            QuietLabel { Layout.preferredWidth: workspace.macroMode ? 72 : 60; text: qsTr("延迟"); font.pixelSize: 11; color: Theme.muted }
                            QuietLabel { Layout.preferredWidth: 64; text: qsTr("操作"); font.pixelSize: 11; color: Theme.muted }
                        }
                    }
                    delegate: Loader {
                        id: rowLoader
                        required property var modelData
                        required property int index
                        width: list.width
                        height: 48
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
                    QuietLabel {
                        anchors.centerIn: parent
                        visible: list.count === 0
                        text: workspace.macroMode ? qsTr("暂无步骤") : qsTr("暂无按键")
                        color: Theme.muted
                    }
                    ScrollBar.vertical: QuietScrollBar {}
                }
                ShortcutBar { Layout.fillWidth: true; page: workspace.page; macroMode: workspace.macroMode }
            }
        }
        ScrollView {
            id: sideScroll
            Layout.preferredWidth: workspace.width < 700 ? 218 : 244
            Layout.fillHeight: true
            contentWidth: availableWidth
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
            ScrollBar.vertical: QuietScrollBar {}
            ColumnLayout {
                width: sideScroll.availableWidth
                spacing: Theme.cardGap
                RunPanel { Layout.fillWidth: true; page: workspace.page }
                HotkeyPanel { Layout.fillWidth: true; page: workspace.page }
                Loader {
                    Layout.fillWidth: true
                    sourceComponent: workspace.macroMode ? macroOptions : repeatOptions
                    Component { id: repeatOptions; RepeatOptions { page: workspace.page } }
                    Component { id: macroOptions; MacroOptions { page: workspace.page } }
                }
            }
        }
    }
}
