import QtQuick
import XiaoJi.UI

Text {
    required property string glyph
    text: glyph
    color: Theme.text
    font.family: Theme.iconFamily
    font.pixelSize: Theme.iconGlyphSize
    font.weight: Font.Normal
    renderType: Text.NativeRendering
    horizontalAlignment: Text.AlignHCenter
    verticalAlignment: Text.AlignVCenter
    Accessible.ignored: true
}
