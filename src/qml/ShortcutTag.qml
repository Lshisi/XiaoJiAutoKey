import QtQuick
import XiaoJi.UI

Rectangle {
    id: root
    required property string text
    property bool removable: true
    property string removeHint: ""
    readonly property real labelWidth: Math.min(Math.ceil(metrics.advanceWidth(text)) + 2, Theme.shortcutTextMaxWidth)
    readonly property int horizontalPadding: removable ? Theme.shortcutTagPadding : 0
    signal removeRequested()
    implicitWidth: labelWidth + (removable ? Theme.shortcutRemoveWidth + Theme.shortcutTagGap : 0)
        + 2 * horizontalPadding
    implicitHeight: Math.max(Theme.compactIconSize, keyLabel.implicitHeight + 4)
    radius: Theme.buttonRadius
    color: removable ? (enabled && tagHover.hovered ? Theme.accentSubtle : Theme.hover) : "transparent"
    border.width: removable && enabled && tagHover.hovered ? 1 : 0
    border.color: Theme.accentBorder
    Accessible.role: removable ? Accessible.Grouping : Accessible.StaticText
    Accessible.name: text
    FontMetrics { id: metrics; font: keyLabel.font }
    HoverHandler {
        id: tagHover
        enabled: root.enabled && root.removable
    }
    QuietLabel {
        id: keyLabel
        anchors.left: parent.left
        anchors.leftMargin: root.horizontalPadding
        anchors.right: root.removable ? removeButton.left : parent.right
        anchors.rightMargin: root.removable ? Theme.shortcutTagGap : 0
        anchors.verticalCenter: parent.verticalCenter
        text: root.text
        color: root.removable && root.enabled ? Theme.text : Theme.muted
        elide: Text.ElideRight
        wrapMode: Text.NoWrap
    }
    IconButton {
        id: removeButton
        anchors.right: parent.right
        anchors.rightMargin: root.horizontalPadding
        anchors.verticalCenter: parent.verticalCenter
        width: Theme.shortcutRemoveWidth
        height: Theme.compactIconSize
        visible: root.removable
        compact: true
        leftPadding: 0
        rightPadding: 0
        topPadding: 0
        bottomPadding: 0
        iconName: "close"
        hint: root.removeHint
        onClicked: root.removeRequested()
    }
}
