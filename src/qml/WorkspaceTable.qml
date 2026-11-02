import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import XiaoJi.UI

Item {
    id: table
    required property QtObject page
    property bool macroMode: false
    readonly property Item guideRow: firstRowAnchor
    property real contentLeftPadding: Theme.cardPadding - 1
    property real contentRightPadding: Theme.cardPadding - 1
    readonly property bool animateRows: rowModel.initialized && !rowModel.replacing && visible && Theme.motionEnabled
    Connections {
        target: onboardingModel
        function onStateChanged() {
            if (!table.macroMode && onboardingModel.state.active && onboardingModel.state.target === "mode")
                list.positionViewAtBeginning()
        }
    }
    WorkspaceRows {
        id: rowModel
        sourceRows: table.macroMode ? table.page.state.steps : table.page.state.bindings
        contextKey: {
            const profiles = table.page.state.profiles
            const index = table.page.state.profileIndex
            return index >= 0 && index < profiles.length ? String(profiles[index].id) : ""
        }
        onContextReset: list.positionViewAtBeginning()
    }
    FontMetrics {
        id: columnMetrics
        font.family: Theme.fontFamily
        font.pixelSize: Theme.bodyFontSize
    }
    QtObject {
        id: tableColumns
        readonly property real rowWidth: Math.max(0, list.width)
        readonly property real contentLeftPadding: table.contentLeftPadding
        readonly property real contentRightPadding: table.contentRightPadding + tableScrollBar.implicitWidth
        readonly property int leadingWidth: table.macroMode ? Theme.iconSize : 42
        readonly property int actionPadding: 4
        readonly property int actionWidth: 2 * Theme.iconSize + 2 * actionPadding
        readonly property real minimumKeyWidth: Math.max(64, columnMetrics.advanceWidth(qsTr("侧键1")) + 22)
        readonly property real minimumModeWidth: Math.max(40, columnMetrics.advanceWidth(qsTr("模式")) + 12)
        readonly property real minimumPriorityWidth: Math.max(34, columnMetrics.advanceWidth(qsTr("优先")) + 10)
        readonly property real minimumDelayWidth: Math.max(44, columnMetrics.advanceWidth(table.macroMode ? qsTr("间隔 (ms)") : qsTr("延迟")) + 12)
        readonly property real dataWidth: Math.max(0, rowWidth - contentLeftPadding - contentRightPadding - leadingWidth - actionWidth)
        readonly property real minimumFieldsWidth: minimumKeyWidth + minimumDelayWidth
            + (table.macroMode ? 0 : minimumModeWidth + minimumPriorityWidth)
        readonly property real nameWidth: Math.min(Math.max(80, columnMetrics.advanceWidth(qsTr("名称")) + 40), Math.max(0, dataWidth - minimumFieldsWidth))
        readonly property real columnExpansion: Math.max(0, dataWidth - minimumFieldsWidth - nameWidth) / (table.macroMode ? 2 : 4)
        readonly property real keyWidth: minimumKeyWidth + columnExpansion
        readonly property real modeWidth: minimumModeWidth + columnExpansion
        readonly property real priorityWidth: minimumPriorityWidth + columnExpansion
        readonly property real delayWidth: minimumDelayWidth + columnExpansion
    }
    ColumnLayout {
        anchors.fill: parent
        spacing: 0
        Item {
            Layout.fillWidth: true
            implicitHeight: Theme.tableHeaderHeight
            Rectangle { anchors.fill: parent; color: Theme.accentSubtle; Accessible.ignored: true }
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: tableColumns.contentLeftPadding
                anchors.rightMargin: tableColumns.contentRightPadding
                spacing: 0
                QuietLabel { Layout.minimumWidth: tableColumns.leadingWidth; Layout.maximumWidth: tableColumns.leadingWidth; text: table.macroMode ? "" : qsTr("状态"); horizontalAlignment: Text.AlignHCenter; color: Theme.muted }
                QuietLabel { Layout.minimumWidth: tableColumns.keyWidth; Layout.maximumWidth: tableColumns.keyWidth; text: qsTr("按键"); horizontalAlignment: Text.AlignHCenter; color: Theme.muted }
                QuietLabel { Layout.minimumWidth: tableColumns.nameWidth; Layout.maximumWidth: tableColumns.nameWidth; text: qsTr("名称"); horizontalAlignment: Text.AlignHCenter; color: Theme.muted; wrapMode: Text.NoWrap; elide: Text.ElideRight }
                QuietLabel { visible: !table.macroMode; Layout.minimumWidth: tableColumns.modeWidth; Layout.maximumWidth: tableColumns.modeWidth; text: qsTr("模式"); horizontalAlignment: Text.AlignHCenter; color: Theme.muted }
                QuietLabel { visible: !table.macroMode; Layout.minimumWidth: tableColumns.priorityWidth; Layout.maximumWidth: tableColumns.priorityWidth; text: qsTr("优先"); horizontalAlignment: Text.AlignHCenter; color: Theme.muted }
                QuietLabel { Layout.minimumWidth: tableColumns.delayWidth; Layout.maximumWidth: tableColumns.delayWidth; text: table.macroMode ? qsTr("间隔 (ms)") : qsTr("延迟"); horizontalAlignment: Text.AlignHCenter; color: Theme.muted }
                QuietLabel { Layout.minimumWidth: tableColumns.actionWidth; Layout.maximumWidth: tableColumns.actionWidth; text: qsTr("操作"); horizontalAlignment: Text.AlignHCenter; color: Theme.muted }
            }
        }
        ListView {
            id: list
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            model: rowModel.replacing ? null : rowModel
            delegate: Loader {
                id: rowLoader
                required property var rowData
                required property int index
                width: tableColumns.rowWidth
                height: Theme.tableRowHeight
                sourceComponent: table.macroMode ? stepDelegate : bindingDelegate
                Component {
                    id: bindingDelegate
                    BindingRow { page: table.page; entry: rowLoader.rowData; columns: tableColumns; enabled: !table.page.state.running }
                }
                Component {
                    id: stepDelegate
                    MacroStepRow { page: table.page; entry: rowLoader.rowData; columns: tableColumns; rowIndex: rowLoader.index; rowCount: list.count; enabled: !table.page.state.running }
                }
            }
            add: Transition {
                enabled: table.animateRows
                OpacityAnimator { from: 0; to: 1; duration: Theme.fadeDuration; easing.type: Easing.OutCubic }
            }
            ScrollBar.vertical: QuietScrollBar { id: tableScrollBar }
        }
    }
    Item {
        id: firstRowAnchor
        x: 0
        y: Theme.tableHeaderHeight
        width: table.width
        height: Math.min(Theme.tableRowHeight, Math.max(0, table.height - y))
        visible: rowModel.count > 0
    }
    Item {
        anchors.fill: parent
        anchors.topMargin: Theme.tableHeaderHeight
        visible: rowModel.count === 0
        ColumnLayout {
            anchors.centerIn: parent
            width: Math.min(260, Math.max(0, parent.width - 2 * Theme.cardPadding))
            spacing: 8
            QuietIcon {
                Layout.alignment: Qt.AlignHCenter
                name: "keyboard"
                pixelSize: 28
                color: Theme.muted
                opacity: 0.6
                Layout.bottomMargin: 4
            }
            QuietLabel {
                Layout.fillWidth: true
                text: table.macroMode ? qsTr("还没有循环步骤") : qsTr("还没有按键")
                font.weight: Theme.headingWeight
                horizontalAlignment: Text.AlignHCenter
            }
            QuietLabel {
                Layout.fillWidth: true
                text: qsTr("点击“添加按键”，开始配置。")
                color: Theme.muted
                horizontalAlignment: Text.AlignHCenter
            }
        }
    }
}
