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
        spacing: 7
        QuietHeading { text: qsTr("启动触发") }
        RowLayout {
            Layout.fillWidth: true
            spacing: 5
            ActionButton {
                Layout.fillWidth: true
                text: root.page.state.hotkey
                enabled: !root.page.state.running
                onClicked: root.page.hotkeyEditClicked()
                hint: qsTr("点击后按下要设置的快捷键")
            }
            IconButton { glyph: "\uE74D"; hint: qsTr("清除启停快捷键"); enabled: !root.page.state.running; onClicked: root.page.hotkeyClearClicked() }
        }
        RowLayout {
            Layout.fillWidth: true
            QuietCheck { text: qsTr("开启提示音"); checked: root.page.state.beepEnabled; onClicked: root.page.beepToggled(checked) }
            Item { Layout.fillWidth: true }
            IconButton { glyph: "\uE767"; hint: qsTr("提示音设置"); onClicked: root.page.soundSettingsClicked() }
        }
        RowLayout {
            Layout.fillWidth: true
            QuietCheck { text: qsTr("显示悬浮窗"); checked: root.page.state.overlayEnabled; onClicked: root.page.statusOverlayToggled(checked) }
            Item { Layout.fillWidth: true }
            IconButton { glyph: "\uE713"; hint: qsTr("悬浮窗设置"); onClicked: root.page.statusOverlaySettingsClicked() }
        }
    }
}
