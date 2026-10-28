import QtQuick
import QtQuick.Layouts
import XiaoJi.UI

ColumnLayout {
    id: editor
    readonly property var fields: dialogModel.dialogState
    spacing: 14
    QuietLabel { text: qsTr("按键") }
    ActionButton { Layout.fillWidth: true; text: editor.fields.keyText; onClicked: dialogModel.dispatch("record") }
    Loader {
        Layout.fillWidth: true
        active: editor.fields.modeVisible
        visible: active
        sourceComponent: ColumnLayout {
            spacing: 12
            QuietLabel { text: qsTr("模式") }
            QuietCombo {
                Layout.fillWidth: true
                model: [qsTr("开关模式"), qsTr("按压模式"), qsTr("独立模式"), qsTr("切换模式")]
                currentIndex: editor.fields.mode
                onActivated: dialogModel.edit("mode", currentIndex)
                Accessible.name: qsTr("连发模式")
            }
            RowLayout {
                Layout.fillWidth: true
                QuietCheck { text: qsTr("启用此按键"); checked: editor.fields.enabled; onClicked: dialogModel.edit("enabled", checked) }
                QuietCheck { visible: editor.fields.mode === 1; text: qsTr("参与按压优先"); checked: editor.fields.priority; onClicked: dialogModel.edit("priority", checked) }
            }
        }
    }
    Loader {
        Layout.fillWidth: true
        active: editor.fields.needsDelay
        visible: active
        sourceComponent: ColumnLayout {
            QuietLabel { text: qsTr("间隔（ms）") }
            QuietTextField {
                Layout.fillWidth: true
                text: editor.fields.delay
                validator: IntValidator { bottom: 1 }
                inputMethodHints: Qt.ImhDigitsOnly
                Accessible.name: qsTr("按键间隔，毫秒")
                onTextEdited: dialogModel.edit("delay", text)
                onActiveFocusChanged: { if (activeFocus) selectAll() }
            }
        }
    }
    Loader {
        Layout.fillWidth: true
        active: editor.fields.isMouse
        visible: active
        sourceComponent: ColumnLayout {
            spacing: 10
            Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Theme.divider }
            QuietHeading { text: qsTr("点击位置") }
            QuietCombo {
                Layout.fillWidth: true
                model: [qsTr("跟随鼠标"), qsTr("固定点位")]
                currentIndex: editor.fields.fixed ? 1 : 0
                onActivated: dialogModel.edit("fixed", currentIndex === 1)
                Accessible.name: qsTr("鼠标点击位置模式")
            }
            QuietLabel { Layout.fillWidth: true; text: editor.fields.supportsPosition ? qsTr("按屏幕比例记录，切换分辨率后自动换算。") : qsTr("当前驱动不支持鼠标移动，无法使用固定点位。"); color: Theme.muted; font.pixelSize: 12 }
            RowLayout {
                visible: editor.fields.fixed
                Layout.fillWidth: true
                QuietLabel { Layout.fillWidth: true; text: editor.fields.positionHint; color: Theme.muted; font.pixelSize: 12 }
                ActionButton { text: qsTr("拾取位置"); enabled: editor.fields.supportsPosition; onClicked: dialogModel.dispatch("pick") }
            }
        }
    }
}
