import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import XiaoJi.UI

ColumnLayout {
    id: editor
    readonly property var fields: dialogModel.dialogState
    spacing: Theme.formGap
    GridLayout {
        Layout.fillWidth: true
        columns: editor.fields.modeVisible ? 4 : 2
        columnSpacing: 12
        rowSpacing: Theme.formGap
        QuietLabel { text: qsTr("按键:") }
        ActionButton { Layout.fillWidth: true; Layout.minimumWidth: 0; text: editor.fields.keyText; onClicked: dialogModel.dispatch("record") }
        QuietLabel { visible: editor.fields.modeVisible; text: qsTr("模式:") }
        QuietCombo {
            visible: editor.fields.modeVisible
            Layout.fillWidth: true
            Layout.minimumWidth: 100
            model: [qsTr("开关模式"), qsTr("按压模式"), qsTr("独立模式"), qsTr("切换模式")]
            currentIndex: editor.fields.mode
            onActivated: dialogModel.edit("mode", currentIndex)
            Accessible.name: qsTr("连发模式")
        }
        QuietLabel { visible: editor.fields.needsDelay; text: qsTr("间隔(ms):") }
        QuietTextField {
            property bool clearedOnClick: false
            visible: editor.fields.needsDelay
            Layout.fillWidth: true
            Layout.columnSpan: editor.fields.modeVisible ? 3 : 1
            text: editor.fields.delay
            validator: IntValidator { bottom: 1; top: 2147483647 }
            inputMethodHints: Qt.ImhDigitsOnly
            Accessible.name: qsTr("按键间隔，毫秒")
            onTextEdited: dialogModel.edit("delay", text)
            onPressed: {
                if (!clearedOnClick) {
                    clearedOnClick = true
                    clear()
                    dialogModel.edit("delay", "")
                }
            }
            onAccepted: dialogModel.dispatch("accept")
        }
        RowLayout {
            visible: editor.fields.modeVisible
            Layout.columnSpan: 2
            Layout.fillWidth: true
            QuietLabel { Layout.fillWidth: true; text: qsTr("启用此按键") }
            QuietSwitch { checked: editor.fields.enabled; onClicked: dialogModel.edit("enabled", checked); Accessible.name: qsTr("启用此按键") }
        }
        RowLayout {
            visible: editor.fields.modeVisible && editor.fields.mode === 1
            Layout.columnSpan: 2
            Layout.fillWidth: true
            QuietLabel { Layout.fillWidth: true; text: qsTr("参与按压优先") }
            QuietSwitch { checked: editor.fields.priority; onClicked: dialogModel.edit("priority", checked); Accessible.name: qsTr("参与按压优先") }
        }
    }
    Loader {
        Layout.fillWidth: true
        active: editor.fields.positionVisible && editor.fields.isMouse
        visible: active
        sourceComponent: ColumnLayout {
            spacing: 6
            ButtonGroup { id: positionGroup }
            QuietLabel { text: qsTr("点击位置"); color: Theme.muted }
            QuietRadio { text: qsTr("跟随鼠标当前位置"); ButtonGroup.group: positionGroup; checked: !editor.fields.fixed; onClicked: dialogModel.edit("fixed", false) }
            RowLayout {
                Layout.fillWidth: true
                QuietRadio { text: qsTr("固定点位"); ButtonGroup.group: positionGroup; checked: editor.fields.fixed; enabled: editor.fields.supportsPosition; onClicked: dialogModel.edit("fixed", true) }
                ActionButton { text: qsTr("拾取坐标"); enabled: editor.fields.fixed && editor.fields.supportsPosition; onClicked: dialogModel.dispatch("pick") }
                Item { Layout.fillWidth: true }
            }
            QuietLabel { Layout.fillWidth: true; text: editor.fields.positionHint; color: Theme.muted }
            QuietLabel { Layout.fillWidth: true; text: editor.fields.supportsPosition ? qsTr("固定点位按屏幕比例记录，切换分辨率后自动换算。") : qsTr("当前驱动不支持鼠标移动，无法使用固定点位。"); color: Theme.muted }
        }
    }
}
