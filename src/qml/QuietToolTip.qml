import QtQuick
import QtQuick.Controls.Basic
import XiaoJi.UI

ToolTip {
    id: control
    delay: 600
    timeout: 5000
    margins: 8
    padding: 8
    implicitWidth: Math.min(280, tipLabel.implicitWidth + leftPadding + rightPadding)
    implicitHeight: tipLabel.implicitHeight + topPadding + bottomPadding
    contentItem: QuietLabel {
        id: tipLabel
        text: control.text
        color: Theme.text
        font.pixelSize: Theme.captionFontSize
        wrapMode: Text.Wrap
    }
    background: Rectangle {
        radius: Theme.buttonRadius
        color: Theme.surface
        border.color: Theme.border
    }
}
