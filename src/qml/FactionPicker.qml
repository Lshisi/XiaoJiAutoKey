import QtQuick
import QtQuick.Layouts
import QtQuick.Effects
import XiaoJi.UI

RowLayout {
    id: root
    spacing: 8
    Item {
        Layout.preferredWidth: 26
        Layout.preferredHeight: 34
        Image {
            id: symbol
            anchors.fill: parent
            source: Theme.colors.factionIcon
            sourceSize: Qt.size(width * Screen.devicePixelRatio, height * Screen.devicePixelRatio)
            fillMode: Image.PreserveAspectFit
            visible: false
        }
        MultiEffect {
            anchors.fill: parent
            source: symbol
            colorization: 1
            colorizationColor: Theme.accent
            visible: symbol.status === Image.Ready
        }
    }
    QuietCombo {
        Layout.fillWidth: true
        Layout.minimumWidth: 64
        implicitWidth: 110
        model: appView.factions
        textRole: "name"
        valueRole: "id"
        currentIndex: {
            const id = Theme.colors.factionId
            for (let i = 0; i < count; ++i) if (valueAt(i) === id) return i
            return -1
        }
        onActivated: appView.selectFaction(currentValue)
        Accessible.name: qsTr("门派主题")
    }
}
