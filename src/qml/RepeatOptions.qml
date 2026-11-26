import QtQuick
import QtQuick.Layouts
import XiaoJi.UI

QuietCard {
    id: root
    required property QtObject page
    horizontalPadding: Theme.controlCardPadding
    implicitHeight: body.implicitHeight + topPadding + bottomPadding
    ColumnLayout {
        id: body
        anchors.fill: parent
        spacing: 4
        QuietHeading { text: qsTr("设置") }
        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: false
            spacing: 4
            QuietCheck { text: qsTr("按压优先"); enabled: !root.page.state.running; checked: root.page.state.mixedMode; onClicked: root.page.mixedModeToggled(checked) }
            QuietLabel { Layout.fillWidth: true; text: qsTr("按住时暂停其他连发。"); color: Theme.muted; font.pixelSize: Theme.bodyFontSize }
        }
        Item {
            id: chickenGap
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: implicitHeight
            implicitHeight: Theme.cardGap
            Rectangle { anchors.left: parent.left; anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter; height: 1; color: Theme.divider }
            Loader {
                anchors.centerIn: parent
                width: 32
                height: 32
                active: Theme.crispyThemeEnabled && chickenGap.height >= 48
                visible: active
                sourceComponent: Rectangle {
                    color: Theme.surface
                    radius: 6
                    QuietIcon { anchors.centerIn: parent; name: "drumstick"; pixelSize: 26; color: Qt.alpha(Theme.crispyGold, 0.4); rotation: -18 }
                    Accessible.ignored: true
                }
            }
        }
        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: false
            spacing: 4
            QuietCheck { text: qsTr("WASD 防卡"); enabled: !root.page.state.running; checked: root.page.state.wasdProtection; onClicked: root.page.wasdProtectionToggled(checked) }
            QuietLabel { Layout.fillWidth: true; text: qsTr("移动降速，松开恢复。"); color: Theme.muted; font.pixelSize: Theme.bodyFontSize }
        }
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: implicitHeight
            implicitHeight: Theme.cardGap
            Rectangle { anchors.left: parent.left; anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter; height: 1; color: Theme.divider }
        }
        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: false
            spacing: 4
            QuietCheck {
                text: qsTr("记住切换位置")
                checked: root.page.state.rememberSequencePosition
                enabled: !root.page.state.running
                onClicked: root.page.rememberSequencePositionChanged(checked)
            }
            QuietLabel { Layout.fillWidth: true; text: qsTr("勾选续用，取消从头。"); color: Theme.muted; font.pixelSize: Theme.bodyFontSize }
        }
    }
}
