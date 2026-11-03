import QtQuick
import QtQuick.Layouts
import XiaoJi.UI

Item {
    id: root
    required property QtObject page
    implicitHeight: timingLabel.implicitHeight

    QuietLabel {
        id: timingLabel
        anchors.left: parent.left
        anchors.right: parent.right
        text: root.page.state.timingText
        color: Theme.muted
        font.pixelSize: Theme.captionFontSize
        elide: Text.ElideMiddle
        wrapMode: Text.NoWrap
        Accessible.name: text
    }
}
