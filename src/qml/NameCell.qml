import QtQuick
import XiaoJi.UI

QuietLabel {
    id: cell
    property string name: ""
    text: name.length ? name : qsTr("—")
    color: name.length ? Theme.text : Theme.muted
    leftPadding: 8
    rightPadding: 8
    horizontalAlignment: Text.AlignHCenter
    verticalAlignment: Text.AlignVCenter
    elide: Text.ElideRight
    wrapMode: Text.NoWrap
    Accessible.name: name.length ? name : qsTr("未命名")
    HoverHandler { id: nameHover }
    QuietToolTip {
        text: cell.name
        visible: cell.visible && nameHover.hovered && cell.truncated && cell.name.length > 0
    }
}
