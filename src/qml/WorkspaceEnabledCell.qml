import QtQuick
import XiaoJi.UI

Item {
    id: cell
    required property QtObject page
    required property var entry
    property bool macroMode: false
    QuietSwitch {
        id: statusSwitch
        anchors.centerIn: parent
        checked: cell.entry.enabled
        Accessible.name: qsTr("启用 %1").arg(cell.entry.keyDisplayText)
        onClicked: cell.macroMode
            ? cell.page.enableStep(cell.entry.id, checked)
            : cell.page.enableBinding(cell.entry.id, checked)
        QuietToolTip {
            visible: statusSwitch.hovered
            text: cell.entry.enabled ? qsTr("已启用，点击停用") : qsTr("已停用，点击启用")
        }
    }
}
