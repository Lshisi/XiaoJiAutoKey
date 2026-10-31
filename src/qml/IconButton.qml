import QtQuick
import XiaoJi.UI

ActionButton {
    required property string glyph
    text: glyph
    textRenderType: Text.NativeRendering
    quiet: true
    implicitWidth: compact ? Theme.compactIconSize : Theme.iconSize
    implicitHeight: compact ? Theme.compactIconSize : Theme.iconSize
    leftPadding: 4
    rightPadding: 4
    topPadding: 4
    bottomPadding: 4
    font.family: Theme.iconFamily
    font.pixelSize: compact ? Theme.smallIconGlyphSize : Theme.iconGlyphSize
    Accessible.name: hint
}
