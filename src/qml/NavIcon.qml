import QtQuick
import QtQuick.Layouts
import XiaoJi.UI

QuietIcon {
    required property string iconId
    required property color iconColor
    Layout.preferredWidth: 20
    Layout.preferredHeight: 20
    pixelSize: Theme.navGlyphSize
    color: iconColor
    name: {
        switch (iconId) {
        case "keys": return "keyboard"
        case "macro": return "repeat"
        case "driver": return "monitor"
        case "settings": return "settings"
        case "about": return "info"
        }
        return ""
    }
}
