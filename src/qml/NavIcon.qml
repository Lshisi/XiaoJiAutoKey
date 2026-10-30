import QtQuick
import QtQuick.Layouts
import XiaoJi.UI

QuietIcon {
    required property string iconId
    required property color iconColor
    Layout.preferredWidth: 20
    Layout.preferredHeight: 20
    font.pixelSize: Theme.navGlyphSize
    color: iconColor
    glyph: {
        switch (iconId) {
        case "keys": return "\uE765"
        case "macro": return "\uE8EF"
        case "driver": return "\uE7F4"
        case "settings": return "\uE713"
        case "about": return "\uE946"
        }
        return ""
    }
}
