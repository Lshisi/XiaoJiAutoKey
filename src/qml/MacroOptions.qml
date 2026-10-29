import QtQuick
import QtQuick.Layouts
import XiaoJi.UI

QuietCard {
    id: root
    required property QtObject page
    horizontalPadding: Theme.controlCardPadding
    implicitHeight: body.implicitHeight + topPadding + bottomPadding
    ColumnLayout {
        id: body
        anchors.fill: parent
        spacing: 6
        QuietHeading { text: qsTr("模式设置") }
        QuietLabel { text: qsTr("触发模式"); color: Theme.muted; font.pixelSize: 12 }
        QuietCombo {
            Layout.fillWidth: true
            model: [qsTr("开关模式"), qsTr("按压模式")]
            currentIndex: root.page.state.triggerMode
            enabled: !root.page.state.running
            onActivated: root.page.modeChanged(currentIndex)
            Accessible.name: qsTr("循环触发方式")
        }
        QuietLabel { visible: root.page.state.triggerMode === 0; text: qsTr("循环次数 (0=无限)"); color: Theme.muted; font.pixelSize: 12; Layout.topMargin: 8 }
        QuietTextField {
            Layout.fillWidth: true
            visible: root.page.state.triggerMode === 0
            text: root.page.state.loopCount.toString()
            validator: IntValidator { bottom: 0; top: 99999 }
            inputMethodHints: Qt.ImhDigitsOnly
            enabled: !root.page.state.running
            onEditingFinished: {
                const count = text.length ? Number(text) : 0
                root.page.loopCountChanged(count)
                text = Qt.binding(() => root.page.state.loopCount.toString())
            }
            onActiveFocusChanged: {
                if (!activeFocus && text.length === 0) {
                    root.page.loopCountChanged(0)
                    text = Qt.binding(() => root.page.state.loopCount.toString())
                }
            }
            Accessible.name: qsTr("循环次数，零为无限循环")
        }
        Item { Layout.fillHeight: true }
    }
}
