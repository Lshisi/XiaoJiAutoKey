pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import XiaoJi.UI

ColumnLayout {
    id: details
    required property var overview
    signal navigationRequested(string targetId)
    readonly property var selected: overview.selectedKey
    spacing: Theme.formGap
    RowLayout {
        Layout.fillWidth: true
        QuietHeading {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            text: qsTr("%1 · %2 项关联").arg(details.selected.text ?? details.selected.label).arg(details.selected.usages.length)
            wrapMode: Text.Wrap
        }
        ActionButton {
            visible: details.selected.usages.length === 0
            text: details.overview.macroMode ? qsTr("添加步骤") : qsTr("添加按键")
            enabled: details.selected.canAdd
            compact: true
            onClicked: details.navigationRequested("add")
        }
    }
    QuietLabel {
        visible: details.selected.usages.length === 0
        Layout.fillWidth: true
        text: details.selected.wheel !== 0 && !details.overview.macroMode
            ? qsTr("滚轮动作可在循环连发页配置。")
            : qsTr("此键尚未配置；添加时会预填所选键位，保存前仍可修改。")
        color: Theme.muted
        wrapMode: Text.Wrap
    }
    Repeater {
        model: details.selected.usages
        delegate: ColumnLayout {
            id: association
            required property var modelData
            Layout.fillWidth: true
            spacing: 4
            Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Theme.divider; Accessible.ignored: true }
            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.formGap
                QuietLabel {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    text: association.modelData.name.length > 0
                        ? qsTr("%1 · %2").arg(association.modelData.role).arg(association.modelData.name)
                        : association.modelData.role
                    wrapMode: Text.Wrap
                }
                QuietLabel { text: association.modelData.status; color: Theme.muted }
                ActionButton {
                    visible: association.modelData.action.length > 0
                    text: qsTr("编辑")
                    compact: true
                    quiet: true
                    enabled: !details.overview.running
                    Accessible.name: qsTr("编辑%1：%2").arg(association.modelData.role).arg(association.modelData.keyText)
                    onClicked: details.navigationRequested(association.modelData.target)
                }
            }
            QuietLabel {
                Layout.fillWidth: true
                text: association.modelData.detail
                wrapMode: Text.Wrap
                color: Theme.muted
            }
        }
    }
}
