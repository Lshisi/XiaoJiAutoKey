import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import XiaoJi.UI

ColumnLayout {
    id: editor
    readonly property var fields: dialogModel.dialogState
    spacing: Theme.formGap

    QuietLabel {
        Layout.fillWidth: true
        text: qsTr("将修改选中的 %1 项；未勾选的字段保留原值。").arg(editor.fields.count)
        color: Theme.muted
        wrapMode: Text.Wrap
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: 12
        QuietCheck {
            text: qsTr("修改启用状态")
            checked: editor.fields.modifyEnabled
            onToggled: dialogModel.edit("modifyEnabled", checked)
        }
        Item { Layout.fillWidth: true }
        QuietSwitch {
            enabled: editor.fields.modifyEnabled
            checked: editor.fields.enabled
            onToggled: dialogModel.edit("enabled", checked)
            Accessible.name: qsTr("批量启用状态")
        }
    }

    RowLayout {
        visible: !editor.fields.macroMode
        Layout.fillWidth: true
        spacing: 12
        QuietCheck {
            text: qsTr("修改模式")
            checked: editor.fields.modifyMode
            onToggled: dialogModel.edit("modifyMode", checked)
        }
        Item { Layout.fillWidth: true }
        QuietCombo {
            Layout.preferredWidth: 150
            enabled: editor.fields.modifyMode
            model: [qsTr("开关模式"), qsTr("按压模式"), qsTr("独立模式"), qsTr("切换模式"), qsTr("长按模式")]
            currentIndex: editor.fields.mode
            onActivated: dialogModel.edit("mode", currentIndex)
            Accessible.name: qsTr("批量连发模式")
        }
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: 12
        QuietCheck {
            text: editor.fields.macroMode ? qsTr("修改动作后间隔(ms)") : qsTr("修改延迟(ms)")
            checked: editor.fields.modifyDelay
            onToggled: dialogModel.edit("modifyDelay", checked)
        }
        Item { Layout.fillWidth: true }
        QuietTextField {
            Layout.preferredWidth: 100
            enabled: editor.fields.modifyDelay
            text: editor.fields.delay
            validator: IntValidator { bottom: editor.fields.macroMode ? 0 : 1; top: 2147483647 }
            inputMethodHints: Qt.ImhDigitsOnly
            Accessible.name: editor.fields.macroMode ? qsTr("批量修改动作后间隔，毫秒") : qsTr("批量间隔，毫秒")
            onTextEdited: dialogModel.edit("delay", text)
            onAccepted: dialogModel.dispatch("accept")
        }
    }

    RowLayout {
        visible: !editor.fields.macroMode
        Layout.fillWidth: true
        spacing: 12
        QuietCheck {
            text: qsTr("修改按压优先")
            checked: editor.fields.modifyPriority
            onToggled: dialogModel.edit("modifyPriority", checked)
        }
        Item { Layout.fillWidth: true }
        QuietSwitch {
            enabled: editor.fields.modifyPriority
            checked: editor.fields.priority
            onToggled: dialogModel.edit("priority", checked)
            Accessible.name: qsTr("批量参与按压优先")
        }
    }

    RowLayout {
        visible: editor.fields.macroMode
        Layout.fillWidth: true
        spacing: 12
        QuietCheck {
            text: qsTr("修改保持/等待时长(ms)")
            checked: editor.fields.modifyDuration
            onToggled: dialogModel.edit("modifyDuration", checked)
        }
        Item { Layout.fillWidth: true }
        QuietTextField {
            Layout.preferredWidth: 100
            enabled: editor.fields.modifyDuration
            text: editor.fields.duration
            validator: IntValidator { bottom: 1; top: 2147483647 }
            onTextEdited: dialogModel.edit("duration", text)
            Accessible.name: qsTr("批量保持或等待时长，毫秒")
        }
    }

    QuietLabel {
        visible: !editor.fields.macroMode
        Layout.fillWidth: true
        text: qsTr("按压优先只应用于按压模式；长按模式不修改连发间隔。")
        color: Theme.muted
        wrapMode: Text.Wrap
    }
    QuietLabel {
        visible: editor.fields.macroMode
        Layout.fillWidth: true
        text: qsTr("动作后间隔不应用于纯等待；时长只应用于保持和等待动作。")
        color: Theme.muted
        wrapMode: Text.Wrap
    }
}
