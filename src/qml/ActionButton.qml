import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import XiaoJi.UI

Button {
    id: control
    property bool primary: false
    property bool quiet: false
    property bool accented: false
    property bool compact: false
    property bool destructive: false
    property string leadingGlyph: ""
    property int textRenderType: Theme.textRenderType
    readonly property int glyphSize: leadingGlyph === "\uE710" || leadingGlyph === "\uE711"
        ? Theme.smallIconGlyphSize : Theme.iconGlyphSize
    property string hint: ""
    readonly property color foregroundColor: !enabled ? Theme.muted
        : destructive ? (Theme.dark ? Theme.background : Theme.field)
        : primary ? Theme.accentForeground : Theme.text
    implicitHeight: Math.max(compact ? Theme.compactButtonHeight : Theme.buttonHeight, contentItem.implicitHeight + topPadding + bottomPadding)
    implicitWidth: Math.max(implicitHeight, contentItem.implicitWidth + leftPadding + rightPadding)
    leftPadding: compact ? 8 : 12
    rightPadding: compact ? 8 : 12
    topPadding: compact ? 4 : 6
    bottomPadding: compact ? 4 : 6
    font.family: Theme.fontFamily
    font.pixelSize: Theme.bodyFontSize
    font.weight: primary ? Theme.headingWeight : Font.Normal
    font.hintingPreference: Font.PreferVerticalHinting
    hoverEnabled: true
    focusPolicy: Qt.TabFocus
    Accessible.name: text.length ? text : hint
    contentItem: RowLayout {
        spacing: control.leadingGlyph.length ? 6 : 0
        QuietIcon {
            visible: control.leadingGlyph.length > 0
            glyph: control.leadingGlyph
            color: caption.color
            font.pixelSize: control.glyphSize
            Layout.preferredWidth: control.glyphSize
            Layout.preferredHeight: control.glyphSize
        }
        Text {
            id: caption
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            text: control.text
            textFormat: Text.PlainText
            font: control.font
            renderType: control.textRenderType
            renderTypeQuality: Theme.textRenderQuality
            color: control.foregroundColor
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
    }
    background: Rectangle {
        radius: Theme.buttonRadius
        color: !control.enabled ? Theme.hover : control.destructive ? (control.down || control.hovered ? Qt.darker(Theme.danger, 1.1) : Theme.danger) : control.primary
            ? (control.down ? Theme.accentPressed : control.hovered ? Theme.accentHover : Theme.accent)
            : control.down ? (control.accented || control.quiet ? Theme.accentBorder : Theme.accentSoft)
            : control.hovered ? (control.accented || control.quiet ? Theme.accentSoft : Theme.accentSubtle)
            : control.accented ? Theme.accentSubtle : control.quiet ? "transparent" : Theme.field
        border.width: control.visualFocus ? 2 : control.primary || (control.quiet && !control.accented) ? 0 : 1
        border.color: control.visualFocus ? Theme.accent
            : control.enabled && (control.hovered || control.down || control.accented) ? Theme.accentBorder : Theme.border
    }
    QuietToolTip {
        text: control.hint
        visible: control.enabled && control.visible && control.hovered && !control.down && text.length > 0
    }
}
