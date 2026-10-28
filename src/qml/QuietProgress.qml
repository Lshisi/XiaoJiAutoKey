import QtQuick
import QtQuick.Controls.Basic
import XiaoJi.UI

ProgressBar {
    id: control
    implicitHeight: 6
    from: 0
    to: 100
    background: Rectangle { color: Theme.border; radius: 3 }
    contentItem: Item {
        Rectangle { width: control.position * parent.width; height: parent.height; radius: 3; color: Theme.accent }
    }
}
