import QtQuick
import QtQuick.Layouts
import XiaoJi.UI

QuietCard {
    id: root
    required property QtObject page
    readonly property bool hasHotkey: page.state.hotkey.length > 0 && page.state.hotkey !== qsTr("未设置") && page.state.hotkey !== qsTr("录制中")
    horizontalPadding: Theme.controlCardPadding
    topPadding: 6
    bottomPadding: 6
    implicitHeight: body.implicitHeight + topPadding + bottomPadding
    ColumnLayout {
        id: body
        anchors.fill: parent
        spacing: 4
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
            IconButton { iconName: "delete"; hint: qsTr("清除启停快捷键"); enabled: !root.page.state.running && root.hasHotkey; onClicked: root.page.hotkeyClearClicked() }
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: 3
            QuietCheck { text: qsTr("开启提示音"); enabled: !root.page.state.running; checked: root.page.state.beepEnabled; onClicked: root.page.beepToggled(checked) }
            Item { Layout.fillWidth: true }
            IconButton { iconName: "music"; hint: qsTr("提示音设置"); enabled: !root.page.state.running; onClicked: root.page.soundSettingsClicked() }
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: 3
            QuietCheck { text: qsTr("显示到浮窗"); enabled: !root.page.state.running; checked: root.page.state.overlayEnabled; onClicked: root.page.statusOverlayToggled(checked) }
            Item { Layout.fillWidth: true }
            IconButton { iconName: "settings"; hint: qsTr("悬浮窗设置"); onClicked: root.page.statusOverlaySettingsClicked() }
        }
    }
}
