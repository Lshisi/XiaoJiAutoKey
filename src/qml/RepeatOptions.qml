import QtQuick
import QtQuick.Layouts
import XiaoJi.UI

QuietCard {
    id: root
    required property QtObject page
    implicitHeight: body.implicitHeight + topPadding + bottomPadding
    ColumnLayout {
        id: body
        anchors.fill: parent
        spacing: 5
        QuietHeading { text: qsTr("设置") }
        QuietCheck { text: qsTr("按压优先"); checked: root.page.state.mixedMode; onClicked: root.page.mixedModeToggled(checked) }
        QuietLabel { Layout.fillWidth: true; text: qsTr("按住按压键时，暂停其他连发。"); color: Theme.muted; font.pixelSize: 12 }
        Rectangle { Layout.fillWidth: true; Layout.topMargin: 4; Layout.bottomMargin: 4; implicitHeight: 1; color: Theme.divider }
        QuietCheck { text: qsTr("WASD 防卡"); checked: root.page.state.wasdProtection; onClicked: root.page.wasdProtectionToggled(checked) }
        QuietLabel { Layout.fillWidth: true; text: qsTr("移动时降速，松开后恢复。"); color: Theme.muted; font.pixelSize: 12 }
        Rectangle { Layout.fillWidth: true; Layout.topMargin: 4; Layout.bottomMargin: 4; implicitHeight: 1; color: Theme.divider }
        QuietCheck {
            text: qsTr("记住切换位置")
            checked: root.page.state.rememberSequencePosition
            enabled: !root.page.state.running
            onClicked: root.page.rememberSequencePositionChanged(checked)
        }
        QuietLabel { Layout.fillWidth: true; text: qsTr("勾选续用，取消从头。"); color: Theme.muted; font.pixelSize: 12 }
    }
}
