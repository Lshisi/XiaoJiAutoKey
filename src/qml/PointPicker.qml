import QtQuick
import QtQuick.Controls.Basic
import XiaoJi.UI

Rectangle {
    id: picker
    readonly property var hint: pickerModel.hintState
    color: Qt.rgba(0, 0, 0, 0.15)
    Rectangle {
        id: panel
        width: Math.min(label.implicitWidth + 24, picker.hint.screenWidth - 16)
        height: label.implicitHeight + 20
        x: picker.hint.x + 18 + width > picker.hint.right ? picker.hint.x - width - 18 : picker.hint.x + 18
        y: picker.hint.y + 18 + height > picker.hint.bottom ? picker.hint.y - height - 18 : picker.hint.y + 18
        radius: 8
        color: picker.hint.surface
        border.color: picker.hint.border
        Label {
            id: label
            anchors.fill: parent
            anchors.margins: 10
            text: picker.hint.text
            textFormat: Text.PlainText
            color: picker.hint.textColor
            font.family: picker.hint.fontFamily
            font.pixelSize: Typography.bodyFontSize
            font.hintingPreference: Typography.fontHintingPreference
            renderType: Typography.textRenderType
            renderTypeQuality: Typography.textRenderQuality
            wrapMode: Text.Wrap
        }
    }
}
