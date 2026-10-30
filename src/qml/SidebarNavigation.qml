import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import XiaoJi.UI

Item {
    id: navigation
    property Item currentButton: null
    implicitHeight: entries.implicitHeight
    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 6
        y: navigation.currentButton ? navigation.currentButton.y : 0
        height: navigation.currentButton ? navigation.currentButton.height : Theme.navItemHeight
        visible: navigation.currentButton !== null
        radius: Theme.buttonRadius
        color: Theme.accentSoft
        Accessible.ignored: true
        Rectangle {
            x: -2
            anchors.verticalCenter: parent.verticalCenter
            width: 3
            height: 16
            radius: 1.5
            color: Theme.accent
        }
    }
    ColumnLayout {
        id: entries
        anchors.left: parent.left
        anchors.right: parent.right
        spacing: 2
        Repeater {
            model: [
                { page: "keys", title: qsTr("按键连发") },
                { page: "macro", title: qsTr("循环连发") },
                { page: "driver", title: qsTr("驱动管理") },
                { page: "settings", title: qsTr("全局设置") },
                { page: "about", title: qsTr("关于软件") }
            ]
            delegate: ActionButton {
                id: navButton
                required property var modelData
                readonly property bool selected: appView.currentPage === modelData.page
                Layout.fillWidth: true
                Layout.leftMargin: 6
                Layout.rightMargin: 6
                implicitHeight: Math.max(Theme.navItemHeight, contentItem.implicitHeight + topPadding + bottomPadding)
                text: modelData.title
                quiet: true
                Accessible.name: modelData.title
                onSelectedChanged: { if (selected) navigation.currentButton = navButton }
                Component.onCompleted: { if (selected) navigation.currentButton = navButton }
                onClicked: appView.selectPage(modelData.page)
                contentItem: RowLayout {
                    spacing: 9
                    NavIcon {
                        iconId: navButton.modelData.page
                        iconColor: navButton.selected ? Theme.accent : Theme.muted
                    }
                    QuietLabel {
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        text: navButton.text
                        color: navButton.selected ? Theme.accent : Theme.text
                        font.weight: navButton.selected ? Theme.headingWeight : Font.Normal
                        elide: Text.ElideRight
                        wrapMode: Text.NoWrap
                    }
                }
                background: Rectangle {
                    radius: Theme.buttonRadius
                    color: navButton.down ? Theme.accentSoft : !navButton.selected && navButton.hovered ? Theme.accentSubtle : "transparent"
                    border.width: navButton.visualFocus ? 2 : 0
                    border.color: Theme.accent
                }
            }
        }
    }
}
