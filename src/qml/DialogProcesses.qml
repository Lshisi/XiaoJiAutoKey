pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import XiaoJi.UI

ColumnLayout {
    id: processes
    readonly property var fields: dialogModel.dialogState.processData
    spacing: Theme.cardGap
    QuietLabel {
        Layout.fillWidth: true
        text: qsTr("剑网3已内置。其他程序可从窗口添加，同一程序的多个窗口都会生效。")
        color: Theme.muted
    }
    RowLayout {
        Layout.fillWidth: true
        GameWindowPicker {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            windows: processes.fields.windows
            onProcessSelected: processName => dialogModel.dispatch("selectProcess", processName)
            onRefreshRequested: dialogModel.dispatch("refreshWindows")
        }
        ActionButton {
            id: manual
            text: qsTr("手动添加")
            checkable: true
            accented: checked
            Accessible.description: qsTr("展开或收起手动填写程序名")
        }
    }
    Loader {
        Layout.fillWidth: true
        active: manual.checked
        visible: active
        sourceComponent: ColumnLayout {
            spacing: 6
            RowLayout {
                Layout.fillWidth: true
                QuietTextField {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    text: processes.fields.processName
                    placeholderText: qsTr("例如 game.exe 或 mumu*")
                    maximumLength: 260
                    onTextEdited: dialogModel.edit("processName", text)
                    onAccepted: { if (text.trim().length) dialogModel.dispatch("add") }
                    Accessible.name: qsTr("手动填写程序名")
                }
                ActionButton { text: qsTr("添加"); enabled: processes.fields.processName.trim().length > 0; onClicked: dialogModel.dispatch("add") }
            }
            QuietLabel { Layout.fillWidth: true; text: qsTr("填写程序名，不用填写路径；末尾加 * 可匹配相同前缀。"); color: Theme.muted }
        }
    }
    Item {
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.minimumHeight: 68
        implicitHeight: Math.min(232, processes.fields.configured.length * 56 + 8)
        Rectangle { anchors.fill: parent; color: Theme.field; radius: Theme.inputRadius; border.color: Theme.border }
        ListView {
            id: list
            anchors.fill: parent
            anchors.margins: 4
            clip: true
            reuseItems: true
            model: processes.fields.configured
            delegate: RowLayout {
                id: entry
                required property var modelData
                required property int index
                width: list.width - 10
                height: Math.max(56, implicitHeight + 8)
                spacing: 10
                Accessible.role: Accessible.Grouping
                Accessible.name: qsTr("%1，%2").arg(modelData.title).arg(modelData.detail)
                GameWindowIcon { Layout.leftMargin: 8; Layout.preferredWidth: 24; Layout.preferredHeight: 24; source: entry.modelData.icon }
                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    spacing: 2
                    QuietLabel { Layout.fillWidth: true; text: entry.modelData.title; font.weight: Theme.headingWeight; elide: Text.ElideRight; wrapMode: Text.NoWrap }
                    QuietLabel { Layout.fillWidth: true; text: entry.modelData.detail; color: Theme.muted; font.pixelSize: Theme.captionFontSize; elide: Text.ElideRight; wrapMode: Text.NoWrap }
                }
                QuietLabel { visible: entry.modelData.builtIn; text: qsTr("内置"); color: Theme.muted }
                IconButton {
                    id: removeButton
                    visible: !entry.modelData.builtIn
                    iconName: "delete"
                    hint: qsTr("移除 %1").arg(entry.modelData.processName)
                    onClicked: dialogModel.dispatch("remove", entry.index)
                }
                HoverHandler { id: rowHover }
                QuietToolTip {
                    text: qsTr("%1\n%2\n%3").arg(entry.modelData.title).arg(entry.modelData.detail)
                        .arg(entry.modelData.builtIn ? qsTr("内置剑网3，不能移除。") : qsTr("同一程序的所有窗口共用此生效范围。"))
                    visible: rowHover.hovered && !removeButton.hovered
                }
            }
            ScrollBar.vertical: QuietScrollBar {}
        }
    }
    QuietLabel {
        Layout.fillWidth: true
        visible: text.length > 0
        text: processes.fields.status
        color: processes.fields.statusError ? Theme.danger : Theme.muted
    }
}
