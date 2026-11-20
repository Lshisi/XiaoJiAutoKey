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
            : root.fields.stage === "recording" ? qsTr("正在录制 · 已记录 %1 次操作").arg(root.fields.count)
            : root.fields.stage === "review" ? root.fields.summary
            : qsTr("开始后有 3 秒准备时间，可切回游戏操作，按 F10 停止。")
        wrapMode: Text.Wrap
    }
    QuietLabel {
        Layout.fillWidth: true
        text: qsTr("像平时一样操作键盘、鼠标和滚轮，软件会记住顺序和中间的停顿。完成后按 F10，查看录制结果，再点“确认”加入循环列表。F10 本身不会被录进去。")
        color: Theme.muted
        wrapMode: Text.Wrap
    }
    QuietCheck {
        visible: root.fields.stage === "ready" || root.fields.stage === "review"
        text: qsTr("记录鼠标左、右键的点击位置")
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
