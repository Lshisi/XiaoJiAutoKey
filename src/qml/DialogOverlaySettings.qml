import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import XiaoJi.UI

ColumnLayout {
    id: settings
    readonly property var fields: dialogModel.dialogState
    readonly property var repeatOptions: [
        { field: "repeatHotkey", label: qsTr("启停快捷键") }, { field: "repeatIndependent", label: qsTr("独立模式") },
        { field: "repeatHold", label: qsTr("按压模式") }, { field: "repeatToggle", label: qsTr("开关模式") },
        { field: "repeatSequence", label: qsTr("切换模式") }, { field: "repeatSequenceHotkey", label: qsTr("切换快捷键") },
        { field: "repeatPauseKeys", label: qsTr("暂停快捷键") }
    ]
    readonly property var macroOptions: [
        { field: "macroHotkey", label: qsTr("触发快捷键") }, { field: "macroTriggerMode", label: qsTr("触发模式") },
        { field: "macroLoopCount", label: qsTr("循环次数") }, { field: "macroPauseKeys", label: qsTr("暂停快捷键") }
    ]
    spacing: Theme.cardGap
    TabBar {
        id: tabs
        Layout.fillWidth: true
        currentIndex: settings.fields.tab
        Repeater {
            model: [qsTr("按键连发"), qsTr("循环连发"), qsTr("通用")]
            delegate: TabButton {
                id: tab
                required property int index
                required property string modelData
                text: modelData
                font.family: Theme.fontFamily
                font.pixelSize: 12
                onClicked: dialogModel.edit("tab", index)
                contentItem: QuietLabel { text: tab.text; horizontalAlignment: Text.AlignHCenter; color: tab.checked ? Theme.accent : Theme.text }
                background: Rectangle {
                    color: tab.hovered ? Theme.hover : "transparent"
                    border.color: tab.visualFocus ? Theme.accent : "transparent"
                    Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 2; color: tab.checked ? Theme.accent : "transparent" }
                }
            }
        }
    }
    Loader {
        Layout.fillWidth: true
        active: settings.fields.tab !== 2
        visible: active
        sourceComponent: ColumnLayout {
            spacing: 10
            Repeater {
                model: settings.fields.tab === 0 ? settings.repeatOptions : settings.macroOptions
                delegate: QuietCheck {
                    required property var modelData
                    text: modelData.label
                    checked: settings.fields[modelData.field]
                    onClicked: dialogModel.edit(modelData.field, checked)
                }
            }
        }
    }
    Loader {
        Layout.fillWidth: true
        active: settings.fields.tab === 2
        visible: active
        sourceComponent: ColumnLayout {
            spacing: 12
            QuietCheck {
                text: qsTr("原生背景模糊")
                checked: settings.fields.nativeBlur
                onClicked: dialogModel.edit("nativeBlur", checked)
            }
            QuietLabel {
                Layout.fillWidth: true
                text: qsTr("柔化浮窗背后的画面，保持文字清晰。")
                color: Theme.muted
                wrapMode: Text.Wrap
            }
            RowLayout {
                Layout.fillWidth: true
                QuietLabel { Layout.fillWidth: true; text: qsTr("背景不透明度") }
                QuietLabel { text: qsTr("%1% ").arg(settings.fields.opacity); color: Theme.muted }
            }
            QuietSlider { Layout.fillWidth: true; from: 30; value: settings.fields.opacity; stepSize: 5; onMoved: dialogModel.edit("opacity", Math.round(value)); Accessible.name: qsTr("悬浮窗背景不透明度") }
            QuietLabel {
                Layout.fillWidth: true
                text: qsTr("数值越高，文字越容易辨认；锁定按钮在浮窗右上角。")
                color: Theme.muted
                wrapMode: Text.Wrap
            }
        }
    }
    Item { Layout.fillHeight: true }
}
