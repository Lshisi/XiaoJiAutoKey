import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import XiaoJi.UI

QuietCard {
    id: guideHint
    property string text: ""
    signal dismissed()
    padding: Theme.controlCardPadding
    implicitHeight: content.implicitHeight + topPadding + bottomPadding
    background: Rectangle {
        radius: Theme.inputRadius
        color: Theme.accentSubtle
        border.color: Theme.accentBorder
    }
    ColumnLayout {
        id: content
        anchors.fill: parent
        spacing: 6
        QuietLabel { Layout.fillWidth: true; text: guideHint.text }
        ActionButton {
            Layout.alignment: Qt.AlignRight
            quiet: true
            text: qsTr("知道了")
            onClicked: guideHint.dismissed()
        }
    }
}
