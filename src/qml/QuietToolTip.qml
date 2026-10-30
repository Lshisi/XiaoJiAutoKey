import QtQuick
import QtQuick.Controls.Basic
import XiaoJi.UI

ToolTip {
    id: control
    delay: 600
    timeout: 5000
    margins: 8
    padding: 8
    implicitWidth: Math.min(280, implicitContentWidth + leftPadding + rightPadding)
    contentItem: QuietLabel {
        text: control.text
        font.pixelSize: Theme.captionFontSize
    }
    background: Rectangle {
        radius: Theme.buttonRadius
        color: Theme.surface
        border.color: Theme.border
    }
}
