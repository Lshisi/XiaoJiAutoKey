import QtQuick
import QtQuick.Controls.Basic
import XiaoJi.UI

MenuItem {
    id: control
    implicitHeight: Math.max(36, contentItem.implicitHeight + 12)
    font.family: Theme.fontFamily
    font.pixelSize: Theme.bodyFontSize
    font.weight: Theme.bodyWeight
    font.hintingPreference: Theme.fontHintingPreference
    hoverEnabled: true
    contentItem: Text {
        text: control.text
        textFormat: Text.PlainText
        font: control.font
        renderType: Theme.textRenderType
        renderTypeQuality: Theme.textRenderQuality
        color: control.enabled ? Theme.bodyText : Theme.muted
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
    background: Rectangle {
        radius: Theme.buttonRadius
        color: !control.enabled ? "transparent" : control.down ? Theme.accentSoft : control.highlighted || control.hovered ? Theme.accentSubtle : "transparent"
        border.width: control.visualFocus ? 1 : 0
        border.color: Theme.accent
    }
}
