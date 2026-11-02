import QtQuick
import XiaoJi.UI

ActionButton {
    id: control
    required property string iconName
    property int iconPixelSize: compact ? Theme.smallIconGlyphSize : Theme.iconGlyphSize
    quiet: true
    implicitWidth: compact ? Theme.compactIconSize : Theme.iconSize
    implicitHeight: compact ? Theme.compactIconSize : Theme.iconSize
    leftPadding: 4
    rightPadding: 4
    topPadding: 4
    bottomPadding: 4
    Accessible.name: hint
    contentItem: QuietIcon {
        name: control.iconName
        color: control.foregroundColor
        pixelSize: control.iconPixelSize
    }
}
