pragma Singleton
import QtQuick

QtObject {
    readonly property var colors: appView.theme
    readonly property bool dark: colors.dark
    readonly property color background: colors.background
    readonly property color surface: colors.surface
    readonly property color surfaceTint: colors.surfaceTint
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
    readonly property int cardRadius: colors.cardRadius
    readonly property int dialogRadius: colors.dialogRadius
    readonly property int titleBarHeight: colors.titleBarHeight
    readonly property int sidebarWidth: colors.sidebarWidth
    readonly property int tableHeaderHeight: colors.tableHeaderHeight
    readonly property int tableRowHeight: colors.tableRowHeight
    readonly property int statusBarHeight: 24
    readonly property int navItemHeight: 36
    readonly property int buttonRadius: colors.buttonRadius
    readonly property int inputRadius: colors.inputRadius
    readonly property int buttonHeight: 32
    readonly property int primaryHeight: 32
    readonly property int iconSize: 32
    readonly property int dialogPadding: 24
    readonly property int dialogVerticalPadding: 18
    readonly property int formGap: 14
    readonly property string fontFamily: "Microsoft YaHei UI"
    readonly property string iconFamily: "Segoe MDL2 Assets"
}
