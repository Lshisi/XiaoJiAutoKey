import QtQuick
import QtQuick.Controls
import XiaoJi.UI

Label {
    color: font.weight >= Theme.headingWeight ? Theme.text : Theme.bodyText
    font.family: Theme.fontFamily
    font.pixelSize: Theme.bodyFontSize
    font.weight: Theme.bodyWeight
    font.hintingPreference: Theme.fontHintingPreference
    renderType: Theme.textRenderType
    renderTypeQuality: Theme.textRenderQuality
    wrapMode: Text.Wrap
    textFormat: Text.PlainText
}
