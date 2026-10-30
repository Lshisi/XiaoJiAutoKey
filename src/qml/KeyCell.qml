import QtQuick
import QtQuick.Controls.Basic
import XiaoJi.UI

Item {
    id: cell
    property string text: ""
    property string description: ""
    implicitHeight: caption.implicitHeight
    QuietLabel {
        id: caption
        anchors.centerIn: parent
        width: Math.min(Math.max(0, parent.width - 8), implicitWidth)
        text: cell.text
        leftPadding: 7
        rightPadding: 7
        topPadding: 2
        bottomPadding: 3
        horizontalAlignment: Text.AlignHCenter
        elide: Text.ElideRight
        wrapMode: Text.NoWrap
        Accessible.description: cell.description
        background: Rectangle {
            radius: 5
            color: Theme.field
            border.color: Theme.border
        }
    }
    HoverHandler { id: keyHover }
    QuietToolTip {
        text: cell.description.length ? qsTr("%1\n%2").arg(cell.text).arg(cell.description) : cell.text
        visible: cell.visible && keyHover.hovered && (cell.description.length > 0 || caption.truncated)
    }
}
