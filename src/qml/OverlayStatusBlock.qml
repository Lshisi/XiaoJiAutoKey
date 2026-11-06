import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import XiaoJi.UI

ColumnLayout {
    id: block
    required property var appearance
    required property string heading
    required property var lines
    required property bool active
    property bool reserveLock: false
    spacing: 1
    RowLayout {
        Layout.fillWidth: true
        Layout.minimumHeight: implicitHeight
        spacing: 8
        Rectangle {
            Layout.preferredWidth: 14
            Layout.preferredHeight: 14
            Layout.minimumWidth: 14
            Layout.maximumWidth: 14
            Layout.minimumHeight: 14
            Layout.maximumHeight: 14
            radius: 7
            color: block.active ? block.appearance.accent : block.appearance.accentBorder
            Accessible.ignored: true
        }
        Label {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            text: block.heading
            textFormat: Text.PlainText
            font.family: block.appearance.fontFamily
            font.pixelSize: block.appearance.fontPixelSize
            font.weight: Typography.headingWeight
            font.hintingPreference: Typography.fontHintingPreference
            color: block.appearance.text
            elide: Text.ElideRight
            renderType: Typography.textRenderType
            renderTypeQuality: Typography.textRenderQuality
        }
        Item { visible: block.reserveLock; Layout.preferredWidth: 24 }
    }
    ColumnLayout {
        visible: block.lines.length > 0
        Layout.fillWidth: true
        Layout.minimumHeight: implicitHeight
        Layout.leftMargin: 22
        spacing: 1
        Repeater {
            model: block.lines
            delegate: Label {
                required property string modelData
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                text: modelData
                textFormat: Text.PlainText
                font.family: block.appearance.fontFamily
                font.pixelSize: block.appearance.fontPixelSize
                font.weight: Font.Normal
                font.hintingPreference: Typography.fontHintingPreference
                color: block.appearance.muted
                elide: Text.ElideRight
                renderType: Typography.textRenderType
                renderTypeQuality: Typography.textRenderQuality
            }
        }
    }
}
