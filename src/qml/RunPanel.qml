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
            enabled: root.pageState.running || root.pageState.profileEnabled
            text: root.pageState.running ? qsTr("■  停止") : qsTr("▶  启动")
            onClicked: root.page.startStopClicked()
            Accessible.name: root.pageState.running ? qsTr("停止连发") : qsTr("启动连发")
        }
        QuietLabel {
            Layout.fillWidth: true
            text: root.pageState.running ? qsTr("状态：运行中") : qsTr("状态：已停止")
            color: root.pageState.running ? Theme.accent : Theme.muted
            font.pixelSize: 12
            Accessible.role: Accessible.StaticText
        }
    }
}
