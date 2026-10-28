import QtQuick
import QtQuick.Controls.Basic
import XiaoJi.UI

Pane {
    id: control
    property bool glass: false
    padding: Theme.cardPadding
    background: Rectangle {
        radius: Theme.cardRadius
        color: control.glass ? Qt.alpha(Theme.surface, 0.96) : Theme.surface
        border.color: Theme.border
        Rectangle {
            visible: control.glass
            x: Theme.cardRadius
            y: 1
            height: 1
            width: Math.max(0, parent.width - 2 * Theme.cardRadius)
            color: Theme.rim
        }
    }
}
