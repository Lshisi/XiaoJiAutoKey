pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import XiaoJi.UI

Flickable {
    id: keyboard
    required property var overview
    property bool detailsOpen: false
    signal keyActivated(string keyId, Item anchor)
    readonly property real unit: Math.max(44, (width + 4) / 23)
    implicitHeight: 6.25 * unit + 8
    contentWidth: 23 * unit - 4
    contentHeight: height
    clip: true
    boundsBehavior: Flickable.StopAtBounds
    flickableDirection: Flickable.HorizontalFlick
    Repeater {
        model: keyboard.overview.keyboard
        delegate: KeyOverviewKey {
            id: keyButton
            required property var modelData
            keyData: modelData
            usage: keyboard.overview.keyStates[modelData.id]
            selected: keyboard.detailsOpen && keyboard.overview.selectedId === modelData.id
            x: modelData.column * keyboard.unit
            y: modelData.row * keyboard.unit
            width: modelData.units * keyboard.unit - 4
            height: modelData.rows * keyboard.unit - 4
            onClicked: keyboard.keyActivated(modelData.id, keyButton)
            onActiveFocusChanged: {
                if (!activeFocus) return
                if (x < keyboard.contentX) keyboard.contentX = x
                else if (x + width > keyboard.contentX + keyboard.width)
                    keyboard.contentX = Math.min(keyboard.contentWidth - keyboard.width, x + width - keyboard.width)
            }
        }
    }
    ScrollBar.horizontal: QuietScrollBar { orientation: Qt.Horizontal }
}
