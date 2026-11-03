import QtQuick
import QtQuick.Layouts
import XiaoJi.UI

ColumnLayout {
    id: root
    readonly property var fields: dialogModel.dialogState
    spacing: Theme.formGap
    QuietLabel {
        Layout.fillWidth: true
        text: root.fields.stage === "countdown" ? qsTr("%1 秒后开始，请切回游戏。F10 停止。 ").arg(root.fields.seconds)
            : root.fields.stage === "recording" ? qsTr("正在录制 · 已记录 %1 个输入事件").arg(root.fields.count)
            : root.fields.stage === "review" ? root.fields.summary
            : qsTr("开始后有 3 秒准备时间，可切回游戏操作，按 F10 停止。")
        wrapMode: Text.Wrap
    }
    QuietLabel {
        Layout.fillWidth: true
        text: qsTr("记录键盘、鼠标键、滚轮和间隔。F10 用于停止，不会录入。停止时会补齐尚未松开的键；确认后追加到当前循环。")
        color: Theme.muted
        wrapMode: Text.Wrap
    }
    QuietCheck {
        visible: root.fields.stage === "ready" || root.fields.stage === "review"
        text: qsTr("记录鼠标点击点位")
        enabled: root.fields.supportsPosition
        checked: root.fields.positions
        onToggled: dialogModel.edit("positions", checked)
    }
    QuietTextArea {
        visible: root.fields.stage === "review"
        Layout.fillWidth: true
        Layout.preferredHeight: 160
        text: root.fields.steps.join("\n")
        readOnly: true
    }
}
