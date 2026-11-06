import QtQuick
import XiaoJi.UI

Rectangle {
    id: root
    required property string text
    property bool removable: true
    property string removeHint: ""
    property bool editable: false
    property string editHint: ""
    readonly property real labelWidth: Math.min(Math.ceil(metrics.advanceWidth(text)) + 2, Theme.shortcutTextMaxWidth)
    readonly property int horizontalPadding: removable ? Theme.shortcutTagPadding : 0
    signal removeRequested()
    signal editRequested()
    implicitWidth: labelWidth + (removable ? Theme.shortcutRemoveWidth + Theme.shortcutTagGap : 0)
        + 2 * horizontalPadding
    implicitHeight: Math.max(Theme.compactIconSize, metrics.height + 4)
    radius: Theme.buttonRadius
    color: removable ? (keyLabel.down ? Theme.accentSoft : enabled && tagHover.hovered ? Theme.accentSubtle : Theme.hover) : "transparent"
    border.width: keyLabel.visualFocus ? 2 : removable && enabled && tagHover.hovered ? 1 : 0
    border.color: keyLabel.visualFocus ? Theme.accent : Theme.accentBorder
    Accessible.role: removable ? Accessible.Grouping : Accessible.StaticText
    Accessible.name: text
    FontMetrics { id: metrics; font: keyLabel.font }
    HoverHandler {
        id: tagHover
        enabled: root.enabled && root.removable
    }
    ActionButton {
        id: keyLabel
        anchors.left: parent.left
        anchors.leftMargin: root.horizontalPadding
        anchors.right: root.removable ? removeButton.left : parent.right
        anchors.rightMargin: root.removable ? Theme.shortcutTagGap : 0
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        text: root.text
        hint: root.editHint
        enabled: root.editable && root.enabled
        leftPadding: 0
        rightPadding: 0
        topPadding: 0
        bottomPadding: 0
        quiet: true
        Accessible.name: root.editHint.length ? root.editHint : root.text
        background: Item {}
        onClicked: root.editRequested()
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
