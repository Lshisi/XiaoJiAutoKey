import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import XiaoJi.UI

ActionButton {
    id: control
    required property string pageId
    property string iconName: ""
    property bool selected: appView.currentPage === pageId
    property bool dragging: false
    quiet: true
    leftPadding: 8
    rightPadding: 2
    implicitHeight: Math.max(Theme.navItemHeight, contentItem.implicitHeight + topPadding + bottomPadding)
    onClicked: appView.selectPage(pageId)
    contentItem: RowLayout {
        spacing: 7
        QuietIcon {
            name: control.iconName
            color: control.selected ? Theme.accent : Theme.muted
            pixelSize: Theme.navGlyphSize
            Layout.preferredWidth: 20
            Layout.preferredHeight: 20
        }
        QuietLabel {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            text: control.text
            color: control.selected ? Theme.accent : Theme.bodyText
            font.weight: control.selected ? Theme.headingWeight : Theme.bodyWeight
            elide: Text.ElideRight
            wrapMode: Text.NoWrap
        }
    }
    background: Rectangle {
        radius: Theme.buttonRadius
        color: control.selected || control.down || control.dragging ? Theme.accentSoft : control.hovered ? Theme.accentSubtle : "transparent"
        border.width: control.visualFocus ? 2 : 0
        border.color: Theme.accent
        Rectangle {
            visible: control.selected
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            width: 3
            height: 16
            radius: 1.5
            color: Theme.accent
        }
    }
    hint: qsTr("%1；拖动排序，或按 Alt+↑/↓ 调整位置").arg(text)
    toolTipHovered: hovered && !dragging
}
