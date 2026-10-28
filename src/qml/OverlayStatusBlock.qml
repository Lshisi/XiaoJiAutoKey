import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: block
    required property var appearance
    required property string heading
    required property var lines
    required property bool active
    property bool reserveLock: false
    spacing: 4
    RowLayout {
        Layout.fillWidth: true
        spacing: 8
        Rectangle {
            Layout.preferredWidth: 7
            Layout.preferredHeight: 7
            radius: 3.5
            color: block.active ? block.appearance.accent : Qt.alpha(block.appearance.muted, 0.65)
            Accessible.ignored: true
        }
        Label {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            text: block.heading
            textFormat: Text.PlainText
            font.family: block.appearance.fontFamily
            font.pixelSize: block.appearance.fontPixelSize
            font.weight: Font.DemiBold
            color: block.appearance.text
            elide: Text.ElideRight
            renderType: Text.NativeRendering
        }
        Item { visible: block.reserveLock; Layout.preferredWidth: 28 }
    }
    ColumnLayout {
        visible: block.lines.length > 0
        Layout.fillWidth: true
        Layout.leftMargin: 15
        spacing: 3
        Repeater {
            model: block.lines
            delegate: Label {
                required property string modelData
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                text: modelData
                textFormat: Text.PlainText
                font.family: block.appearance.fontFamily
                font.pixelSize: block.appearance.fontPixelSize - 1
                color: block.appearance.muted
                elide: Text.ElideRight
                renderType: Text.NativeRendering
            }
        }
    }
}
