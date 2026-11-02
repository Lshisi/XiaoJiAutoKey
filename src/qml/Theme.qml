pragma Singleton
import QtQuick

QtObject {
    readonly property var colors: appView.theme
    readonly property bool dark: colors.dark
    readonly property color background: colors.background
    readonly property color surface: colors.surface
    readonly property color surfaceTint: colors.surfaceTint
    readonly property color titleBarBackground: colors.titleBarBackground
    readonly property color field: colors.field
    readonly property color border: colors.border
    readonly property color text: colors.text
    readonly property color muted: colors.muted
    readonly property color hover: colors.hover
    readonly property color divider: colors.divider
    readonly property color accent: colors.accent
    readonly property color accentHover: colors.accentHover
    readonly property color accentPressed: colors.accentPressed
    readonly property color accentSoft: colors.accentSoft
    readonly property color accentSubtle: colors.accentSubtle
    readonly property color accentBorder: colors.accentBorder
    readonly property color accentForeground: colors.accentForeground
    readonly property color danger: colors.danger
    readonly property color warning: colors.warning
    readonly property color success: colors.success
    readonly property color rim: dark ? Qt.rgba(1, 1, 1, 0.06) : Qt.rgba(1, 1, 1, 0.85)
    readonly property int pageMargin: colors.pageMargin
    readonly property int cardGap: colors.cardGap
    readonly property int cardPadding: colors.cardPadding
    readonly property int controlCardPadding: 10
    readonly property int cardRadius: colors.cardRadius
    readonly property int dialogRadius: colors.dialogRadius
    readonly property int titleBarHeight: colors.titleBarHeight
    readonly property int sidebarWidth: colors.sidebarWidth
    readonly property int controlPanelWidth: colors.controlPanelWidth
    readonly property int tableHeaderHeight: colors.tableHeaderHeight
    readonly property int tableRowHeight: colors.tableRowHeight
    readonly property int statusBarHeight: 24
    readonly property int navItemHeight: 36
    readonly property int buttonRadius: colors.buttonRadius
    readonly property int inputRadius: colors.inputRadius
    readonly property int buttonHeight: 32
    readonly property int compactButtonHeight: 28
    readonly property int primaryHeight: 32
    readonly property int iconSize: 32
    readonly property int compactIconSize: 24
    readonly property int smallIconGlyphSize: 10
    readonly property int shortcutTextMaxWidth: 72
    readonly property int shortcutRemoveWidth: 18
    readonly property int shortcutTagPadding: 6
    readonly property int shortcutTagGap: 6
    readonly property int dialogPadding: 24
    readonly property int dialogVerticalPadding: 18
    readonly property int formGap: 14
    readonly property int bodyFontSize: 12
    readonly property int captionFontSize: 11
    readonly property int cardTitleFontSize: 15
    readonly property int pageTitleFontSize: 19
    readonly property int iconGlyphSize: 12
    readonly property int navGlyphSize: 16
    readonly property int headingWeight: Font.Medium
    readonly property int textRenderType: Text.QtRendering
    readonly property int textRenderQuality: Text.HighRenderTypeQuality
    readonly property bool motionEnabled: colors.motionEnabled && Qt.application.state === Qt.ApplicationActive
    readonly property int fadeDuration: Math.min(colors.fadeDuration, 100)
    readonly property int toggleDuration: 90
    readonly property int popupEnterDuration: 50
    readonly property int popupExitDuration: 30
    readonly property string fontFamily: "Microsoft YaHei UI"
}
