import QtQuick
import XiaoJi.UI

ActionButton {
    required property string glyph
    text: glyph
    quiet: true
    implicitWidth: compact ? Theme.compactIconSize : Theme.iconSize
    implicitHeight: compact ? Theme.compactIconSize : Theme.iconSize
    leftPadding: 4
    rightPadding: 4
    topPadding: 4
    bottomPadding: 4
    font.family: Theme.iconFamily
    font.pixelSize: glyph === "\uE711" || glyph === "\uE710" ? Theme.smallIconGlyphSize : 14
    Accessible.name: hint
}
