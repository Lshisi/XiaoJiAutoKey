import QtQuick
import XiaoJi.UI

ActionButton {
    required property string glyph
    text: glyph
    quiet: true
    implicitWidth: Theme.iconSize
    implicitHeight: Theme.iconSize
    leftPadding: 4
    rightPadding: 4
    topPadding: 4
    bottomPadding: 4
    font.family: Theme.iconFamily
    font.pixelSize: 14
    Accessible.name: hint
}
