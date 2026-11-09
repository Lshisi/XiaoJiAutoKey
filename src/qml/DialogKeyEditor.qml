import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import XiaoJi.UI

ColumnLayout {
    id: editor
    readonly property var fields: dialogModel.dialogState
    readonly property bool macroStep: editor.fields.macroStep === true
    readonly property real labelWidth: Math.max(62, keyLabel.implicitWidth, modeLabel.implicitWidth,
        delayLabel.implicitWidth, nameLabel.implicitWidth)
    spacing: Theme.formGap
    RowLayout {
        visible: editor.macroStep
        Layout.fillWidth: true
        spacing: 12
        QuietLabel { Layout.minimumWidth: editor.labelWidth; Layout.maximumWidth: editor.labelWidth; text: qsTr("动作:") }
        QuietCombo {
            Layout.fillWidth: true
            model: [qsTr("点击"), qsTr("保持"), qsTr("按下"), qsTr("松开"), qsTr("等待"), qsTr("滚轮")]
            currentIndex: editor.macroStep ? editor.fields.action : 0
            onActivated: dialogModel.edit("action", currentIndex)
            Accessible.name: qsTr("循环动作类型")
        }
    }
    RowLayout {
        visible: editor.fields.modeVisible
        Layout.fillWidth: true
        Layout.minimumWidth: 0
        spacing: 12
        QuietLabel {
            id: modeLabel
            Layout.minimumWidth: editor.labelWidth
            Layout.maximumWidth: editor.labelWidth
            text: qsTr("模式:")
            wrapMode: Text.NoWrap
        }
        QuietCombo {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            model: [qsTr("开关模式"), qsTr("按压模式"), qsTr("独立模式"), qsTr("切换模式"), qsTr("长按模式")]
            currentIndex: editor.fields.mode
            onActivated: dialogModel.edit("mode", currentIndex)
            Accessible.name: qsTr("连发模式")
        }
    }
    Loader {
        Layout.fillWidth: true
        active: editor.macroStep && (editor.fields.action === 1
            || editor.fields.action === 4 || editor.fields.action === 5)
        visible: active
        sourceComponent: DialogMacroParameters { fields: editor.fields; labelWidth: editor.labelWidth }
    }
    RowLayout {
        Layout.fillWidth: true
        Layout.minimumWidth: 0
        visible: !editor.macroStep || editor.fields.action < 4
        spacing: 12
        QuietLabel {
            id: keyLabel
            Layout.minimumWidth: editor.labelWidth
            Layout.maximumWidth: editor.labelWidth
            text: qsTr("按键:")
            wrapMode: Text.NoWrap
        }
        ActionButton {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            text: editor.fields.keyText
            textAlignment: Text.AlignLeft
            leftPadding: 10
            rightPadding: 10
            onClicked: dialogModel.dispatch("record")
        }
    }
    QuietLabel {
        Layout.fillWidth: true
        visible: editor.fields.captureHint !== undefined && (!editor.macroStep || editor.fields.action < 4)
        text: editor.fields.captureHint
        color: Theme.muted
        wrapMode: Text.WordWrap
    }
    RowLayout {
        visible: editor.fields.needsDelay && (!editor.macroStep || editor.fields.action !== 4)
        Layout.fillWidth: true
        spacing: 12
        QuietLabel {
            id: delayLabel
            Layout.minimumWidth: editor.labelWidth
            Layout.maximumWidth: editor.labelWidth
            text: editor.macroStep ? qsTr("动作后间隔(ms):") : qsTr("间隔(ms):")
            wrapMode: Text.NoWrap
        }
        QuietTextField {
            property bool clearedOnClick: false
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            text: editor.fields.delay
            validator: IntValidator { bottom: editor.macroStep ? 0 : 1; top: 2147483647 }
            inputMethodHints: Qt.ImhDigitsOnly
            Accessible.name: editor.macroStep ? qsTr("循环步骤动作后间隔，毫秒") : qsTr("按键间隔，毫秒")
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
    }
    RowLayout {
        visible: editor.fields.nameVisible
        Layout.fillWidth: true
        spacing: 12
        QuietLabel {
            id: nameLabel
            Layout.minimumWidth: editor.labelWidth
            Layout.maximumWidth: editor.labelWidth
            text: qsTr("名称:")
            wrapMode: Text.NoWrap
        }
        QuietTextField {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            text: editor.fields.name
            placeholderText: qsTr("选填，例如单体宏")
            Accessible.name: qsTr("按键名称")
            onTextEdited: dialogModel.edit("name", text)
        }
    }
    RowLayout {
        visible: editor.fields.modeVisible
        Layout.fillWidth: true
        uniformCellSizes: true
        spacing: 12
        RowLayout {
            Layout.fillWidth: true
            QuietLabel { Layout.fillWidth: true; text: qsTr("启用此按键"); wrapMode: Text.NoWrap }
            QuietSwitch { checked: editor.fields.enabled; onClicked: dialogModel.edit("enabled", checked); Accessible.name: qsTr("启用此按键") }
        }
        RowLayout {
            Layout.fillWidth: true
            QuietLabel { visible: editor.fields.mode === 1; Layout.fillWidth: true; text: qsTr("参与按压优先"); wrapMode: Text.NoWrap }
            QuietSwitch { visible: editor.fields.mode === 1; checked: editor.fields.priority; onClicked: dialogModel.edit("priority", checked); Accessible.name: qsTr("参与按压优先") }
        }
    }
    QuietLabel {
        Layout.fillWidth: true
        visible: editor.fields.modeVisible
        text: {
            switch (editor.fields.mode) {
            case 1: return qsTr("先启动工具，再按住此键连发，松开停止。")
            case 2: return qsTr("先启动工具，再按此键独立开启或停止连发。")
            case 3: return qsTr("启动后，通过列表下方的“切换键”轮换当前连发按键。")
            case 4: return qsTr("启动后持续按住，暂停或停止时松开；无需连发间隔。")
            default: return qsTr("启动后按设定间隔反复点击此键，停止时结束。")
            }
        }
        color: Theme.muted
        wrapMode: Text.Wrap
    }
    QuietLabel {
        Layout.fillWidth: true
        visible: editor.macroStep
        text: {
            switch (editor.fields.action) {
            case 1: return qsTr("按住指定时长后松开，再等待动作后间隔。")
            case 2: return qsTr("持续按住此键，须在后续步骤添加对应的“松开”。")
            case 3: return qsTr("松开本循环此前按下的同一按键。")
            case 4: return qsTr("只等待指定时长，不发送按键。")
            case 5: return qsTr("按指定方向滚动，再等待动作后间隔。")
            default: return qsTr("按下约 5ms 后松开，再等待动作后间隔；末步间隔也计入循环。")
            }
        }
        color: Theme.muted
        wrapMode: Text.Wrap
    }
    Loader {
        Layout.fillWidth: true
        active: editor.fields.modeGuideVisible
        visible: active
        sourceComponent: GuideHint {
            text: editor.fields.mode === 2
                ? qsTr("多个独立模式按键之间会互相切换，同一时间只连发当前选中的一个；再次按当前键会停止。其他模式不受这个切换影响。")
                : qsTr("保存后，在按键列表下方设置“切换键”。每按一次，轮换到下一个切换模式按键；右侧“记住切换位置”决定重新启动时是否续用上次位置。")
            onDismissed: dialogModel.dispatch("dismissModeGuide")
        }
    }
    Loader {
        Layout.fillWidth: true
        active: editor.fields.positionVisible && editor.fields.isMouse
            && (!editor.macroStep || editor.fields.action < 3)
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
