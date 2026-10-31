import QtQuick
import QtQuick.Controls
import XiaoJi.UI

Label {
    color: Theme.text
    font.family: Theme.fontFamily
    font.pixelSize: Theme.bodyFontSize
    font.weight: Font.Normal
    font.hintingPreference: Font.PreferVerticalHinting
    renderType: Theme.textRenderType
    renderTypeQuality: Theme.textRenderQuality
    wrapMode: Text.Wrap
    textFormat: Text.PlainText
}
