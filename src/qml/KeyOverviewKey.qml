pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic
import XiaoJi.UI

Button {
    id: control
    CrispyControlFeedback { targetControl: control }
    required property var keyData
    required property var usage
    property bool selected: false
    readonly property bool paused: usage.paused && !usage.active
    readonly property bool colored: usage.configured && usage.enabled && !paused
    readonly property var role: Theme.keyOverviewRoles[usage.kind]
    readonly property color roleColor: role.color
    readonly property color foreground: usage.configured && !usage.enabled ? Theme.muted : Theme.bodyText
    readonly property string statusText: usage.active ? qsTr("执行") : paused ? qsTr("暂停")
        : usage.configured && !usage.enabled ? qsTr("停用") : usage.configured ? role.label : ""
    readonly property string runtimeText: usage.active ? qsTr("运行中") : paused ? qsTr("已暂停")
        : usage.configured ? (usage.enabled ? qsTr("已配置") : qsTr("已停用")) : qsTr("未配置")
    readonly property string hintText: qsTr("%1\n用途：%2\n%3 项关联 · %4")
        .arg(keyData.text ?? keyData.label).arg(usage.description).arg(usage.count).arg(runtimeText)
    implicitHeight: 42
    implicitWidth: 42
    padding: 3
    hoverEnabled: true
    focusPolicy: Qt.TabFocus
    Accessible.name: hintText
    contentItem: Item {
        Column {
            anchors.centerIn: parent
            width: parent.width
            spacing: 1
            QuietLabel {
                width: parent.width
                text: control.keyData.label
                color: control.foreground
                horizontalAlignment: Text.AlignHCenter
                font.pixelSize: control.width < 45 && text.length > 4 ? Theme.captionFontSize : Theme.bodyFontSize
                elide: Text.ElideRight
                wrapMode: Text.NoWrap
            }
            QuietLabel {
                visible: text.length > 0
                width: parent.width
                text: control.statusText
                color: control.colored ? control.foreground : control.paused ? Theme.warning : Theme.muted
                horizontalAlignment: Text.AlignHCenter
                font.pixelSize: Theme.captionFontSize
                elide: Text.ElideRight
                wrapMode: Text.NoWrap
            }
        }
    }
    background: Rectangle {
        radius: Theme.buttonRadius
        color: control.colored ? Qt.tint(Theme.field, Qt.rgba(control.roleColor.r, control.roleColor.g, control.roleColor.b,
            control.down || control.usage.active ? 0.38 : control.hovered ? 0.30 : Theme.dark ? 0.25 : 0.18))
            : control.down ? Theme.accentSoft : control.hovered ? Theme.keyOverviewHover : Theme.keyOverviewFill
        border.width: control.usage.active || control.paused ? 2 : 1
        border.color: control.paused ? Theme.warning : control.colored ? control.roleColor
            : control.down ? Theme.accent : control.hovered ? Theme.keyOverviewBorderHover : Theme.keyOverviewBorder
        Rectangle {
            anchors.fill: parent
            anchors.margins: 3
            visible: control.visualFocus || control.selected
            color: "transparent"
            radius: Math.max(0, Theme.buttonRadius - 3)
            border.width: 2
            border.color: Theme.text
        }
    }
    QuietToolTip {
        visible: control.hovered && !control.selected && text.length > 0
        text: control.hintText
    }
}
