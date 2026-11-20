import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import XiaoJi.UI

ColumnLayout {
    id: editor
    readonly property var fields: dialogModel.dialogState
    readonly property bool macroStep: editor.fields.macroStep === true
    readonly property bool martialAssistEnabled: editor.fields.mixedMode === true && editor.fields.martialAssist === true
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
            visible: editor.fields.mode === 1
            QuietLabel { Layout.fillWidth: true; text: qsTr("参与按压优先"); wrapMode: Text.NoWrap }
            QuietSwitch { checked: editor.fields.priority; onClicked: dialogModel.edit("priority", checked); Accessible.name: qsTr("参与按压优先") }
        }
        RowLayout {
            Layout.fillWidth: true
            visible: editor.fields.mode === 2
            QuietLabel { Layout.fillWidth: true; text: qsTr("防误停") }
            QuietSwitch {
                checked: editor.fields.preventAccidentalStop === true
                onClicked: dialogModel.edit("preventAccidentalStop", checked)
                Accessible.name: qsTr("防误停")
            }
        }
        RowLayout {
            id: martialAssistRow
            Layout.fillWidth: true
            visible: editor.fields.mode === 4
            QuietLabel { Layout.fillWidth: true; text: qsTr("武学助手") }
            QuietSwitch {
                checked: editor.fields.martialAssist === true
                enabled: editor.fields.mixedMode === true || checked
                onClicked: dialogModel.edit("martialAssist", checked)
                Accessible.name: qsTr("武学助手")
                Accessible.description: martialAssistRow.ToolTip.text
            }
            HoverHandler { id: martialAssistHover }
            ToolTip.visible: martialAssistHover.hovered
            ToolTip.text: editor.fields.mixedMode === true ? qsTr("帮你按住 3.1 秒再松开，用来开启游戏里的长按连发。")
                : qsTr("请先在右侧打开“按压优先”，再使用武学助手。")
        }
    }
    QuietLabel {
        Layout.fillWidth: true
        visible: editor.fields.modeVisible
        text: {
            switch (editor.fields.mode) {
            case 1: return qsTr("先启动工具，再用手按住这个键连点，松开就停。")
            case 2: return editor.fields.preventAccidentalStop === true
                ? qsTr("先启动工具，按一下这个键开始连点；再按也不会停止。")
                : qsTr("先启动工具，按一下这个键开始连点，再按一下停止。")
            case 3: return qsTr("启动后，用列表下方的“切换键”更换正在连点的按键。")
            case 4: return editor.martialAssistEnabled
                ? qsTr("启动后，软件会帮你按住 3.1 秒，再松开。")
                : qsTr("启动后，软件会一直按住这个键，暂停或停止时松开。")
            default: return qsTr("点击右侧“启动”，就会自动反复按这个键；点击“停止”结束。")
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
            case 1: return qsTr("按住这个键到设定时间，再松开，等待后继续下一步。")
            case 2: return qsTr("按住这个键不松开，继续执行后面的步骤。")
            case 3: return qsTr("松开前面一直按住的这个键。")
            case 4: return qsTr("在这里等一会儿，不按任何键，再继续下一步。")
            case 5: return qsTr("滚动鼠标滚轮，等待后继续下一步。")
            default: return qsTr("按一下这个键，等下面设置的间隔，再继续下一步。")
            }
        }
        color: Theme.muted
        wrapMode: Text.Wrap
    }
    Loader {
        Layout.fillWidth: true
        active: editor.fields.modeGuideVisible || editor.fields.actionGuideVisible === true
        visible: active
        sourceComponent: GuideHint {
            text: {
                if (editor.macroStep) {
                    switch (editor.fields.action) {
                    case 1: return qsTr("例如想按住 W 一秒：选择“保持”，录制 W，时长填 1000。到时间会自动松开，再等下面设置的间隔。")
                    case 2: return qsTr("例如先用“按下”按住 W，再执行其他步骤。要松开 W 时，在后面添加一条“松开”，也录制 W。只想按住一小会儿，选“保持”更方便。")
                    case 3: return qsTr("如果前面用“按下”让 W 一直按着，这里选择“松开”并录制 W，就会把它松开。这里的键要和前面按住的键一致。")
                    case 4: return qsTr("只想等一会儿、不按任何键，就选“等待”。填 1000，就是等 1 秒。两次按键之间的停顿，也可以直接填在前一步的间隔里。")
                    case 5: return qsTr("选好向上或向下，再填滚动几格即可，不用录制按键。滚动完成后，会等下面设置的间隔再继续。")
                    default: return qsTr("例如想按 1、等一秒、再按 2：添加“点击”并录制 1，间隔填 1000，再添加“点击”并录制 2。最后一步的间隔等完后，才会开始下一轮。")
                    }
                }
                switch (editor.fields.mode) {
                case 1: return qsTr("例如添加按键 1：先启动工具，用手按住 1 就会连续按 1，松开就停。\n如果这项参与“按压优先”，按住时会先暂停其他连发，松开后再继续。")
                case 2: return qsTr("例如添加 1、2 两个独立模式按键。启动后按 1 就连点 1，按 2 就改为连点 2。\n%1").arg(editor.fields.preventAccidentalStop === true
                    ? qsTr("“防误停”已开启，重复按同一个键不会停止；请用右侧“停止”或启停快捷键结束。")
                    : qsTr("再按一下正在连点的键，就会停止；右侧“停止”也能结束连发。"))
                case 3: return qsTr("例如把 1、2、3 都设为切换模式，再到列表下方设置“切换键”。启动后会连点其中一个，每按一次切换键，就按 1 → 2 → 3 的顺序更换，再回到 1。")
                case 4: return editor.martialAssistEnabled
                    ? qsTr("先在游戏里开启“长按连发”，再打开这里的武学助手。启动后，软件会帮你按住这个键 3.1 秒再松开。\n游戏开始连发后，暂停、停止或用按压模式接管时，软件会再按一下这个键，帮你关闭；继续时重新按住 3.1 秒。\n例如用 Alt 暂停 F：会先短暂松开 Alt，单独按一下 F，再恢复你仍按着的 Alt。暂停会保持到你松开 Alt。\n游戏连发时手动按 F，会先由你控制；松开后，没有暂停就重新长按 3.1 秒。如果你自己已经长按满 3.1 秒，就直接继续游戏连发。")
                    : qsTr("例如设置 W：启动后软件会一直按住 W，你不用自己按着；暂停或停止时会自动松开。它不会反复点击，不用设置连点间隔。")
                default: return qsTr("例如想让 1 一直自动连点，就添加按键 1，选择“开关模式”。点击右侧“启动”后会不停按 1，点击“停止”就结束。\n这里录制的是要自动按的键，启动和停止的快捷键在主界面右侧设置。")
                }
            }
            onDismissed: dialogModel.dispatch(editor.macroStep ? "dismissActionGuide" : "dismissModeGuide")
        }
    }
    Loader {
        Layout.fillWidth: true
        active: editor.fields.positionVisible && editor.fields.canPosition === true
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
            QuietLabel { Layout.fillWidth: true; text: editor.fields.supportsPosition ? qsTr("想一直点击同一个地方，先选“固定点位”，再点“拾取坐标”选择位置。") : qsTr("当前驱动不能把鼠标移到指定位置，请先使用“跟随鼠标当前位置”。"); color: Theme.muted }
        }
    }
}
