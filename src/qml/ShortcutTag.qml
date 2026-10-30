import QtQuick
import QtQuick.Layouts
import XiaoJi.UI

Rectangle {
    id: root
    required property string text
    property bool removable: true
    property string removeHint: ""
    signal removeRequested()
    implicitWidth: Math.min(keyLabel.implicitWidth, Theme.shortcutTextMaxWidth)
        + (removable ? Theme.shortcutRemoveWidth : 0) + 2 * Theme.shortcutTagPadding
    implicitHeight: Math.max(Theme.compactIconSize, keyLabel.implicitHeight + 4)
    radius: Theme.buttonRadius
    color: enabled && tagHover.hovered ? Theme.accentSubtle : "transparent"
    border.color: enabled && tagHover.hovered ? Theme.accentBorder : Theme.border
    Accessible.role: Accessible.Grouping
    Accessible.name: text
    HoverHandler {
        id: tagHover
        enabled: root.enabled
    }
    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Theme.shortcutTagPadding
        anchors.rightMargin: Theme.shortcutTagPadding
        spacing: 0
        QuietLabel {
            id: keyLabel
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            text: root.text
            color: root.enabled ? Theme.text : Theme.muted
            elide: Text.ElideRight
            wrapMode: Text.NoWrap
        }
        IconButton {
            visible: root.removable
            compact: true
            implicitWidth: Theme.shortcutRemoveWidth
            leftPadding: 0
            rightPadding: 0
            topPadding: 0
            bottomPadding: 0
            glyph: "\uE711"
            hint: root.removeHint
            onClicked: root.removeRequested()
        }
    }
}
