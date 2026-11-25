import QtQuick
import XiaoJi.UI

Item {
    id: root
    implicitHeight: 1
    Accessible.ignored: true
    Row {
        spacing: 4
        Repeater {
            model: Math.floor((root.width + 4) / 8)
            Rectangle { width: 4; height: 1; color: Qt.alpha(Theme.crispyMuted, 0.4) }
        }
    }
}
