import QtQuick
import QtQuick.Layouts
import XiaoJi.UI

QuietCard {
    id: root
    required property QtObject page
    implicitHeight: body.implicitHeight + topPadding + bottomPadding
    ColumnLayout {
        id: body
        anchors.fill: parent
        spacing: 9
        QuietHeading { text: qsTr("循环设置") }
        QuietLabel { text: qsTr("触发方式"); color: Theme.muted; font.pixelSize: 12 }
        QuietCombo {
            Layout.fillWidth: true
            model: [qsTr("按下切换"), qsTr("按住运行")]
            currentIndex: root.page.state.triggerMode
            enabled: !root.page.state.running
            onActivated: root.page.modeChanged(currentIndex)
            Accessible.name: qsTr("循环触发方式")
        }
        QuietLabel { text: qsTr("循环次数"); color: Theme.muted; font.pixelSize: 12; Layout.topMargin: 4 }
        QuietNumber {
            Layout.fillWidth: true
            value: root.page.state.loopCount
            enabled: !root.page.state.running
            onValueModified: root.page.loopCountChanged(value)
            Accessible.name: qsTr("循环次数，零为无限循环")
        }
        QuietLabel { Layout.fillWidth: true; text: qsTr("设为 0 时无限循环。"); color: Theme.muted; font.pixelSize: 12 }
    }
}
