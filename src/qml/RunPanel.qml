import QtQuick
import QtQuick.Layouts
import XiaoJi.UI

QuietCard {
    id: root
    required property QtObject page
    readonly property var pageState: page.state
    glass: true
    implicitHeight: body.implicitHeight + topPadding + bottomPadding
    ColumnLayout {
        id: body
        anchors.fill: parent
        spacing: 10
        QuietHeading { text: qsTr("快捷控制") }
        ActionButton {
            Layout.fillWidth: true
            implicitHeight: Theme.primaryHeight
            primary: true
            destructive: root.pageState.running
            enabled: root.pageState.running || root.pageState.profileEnabled
            text: {
                const action = root.pageState.running ? qsTr("■  停止") : qsTr("▶  启动")
                const key = root.pageState.hotkey
                return key.length && key !== qsTr("未设置") && key !== qsTr("录制中")
                    ? qsTr("%1 (%2)").arg(action).arg(key) : action
            }
            onClicked: root.page.startStopClicked()
            Accessible.name: root.pageState.running ? qsTr("停止连发") : qsTr("启动连发")
        }
    }
}
