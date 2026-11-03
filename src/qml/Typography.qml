pragma Singleton
import QtQuick

QtObject {
    readonly property string fontFamily: "Microsoft YaHei UI"
    readonly property int bodyFontSize: 12
    readonly property int captionFontSize: 11
    readonly property int cardTitleFontSize: 15
    readonly property int pageTitleFontSize: 19
    readonly property int headingWeight: Font.Medium
    readonly property int fontHintingPreference: Font.PreferVerticalHinting
    readonly property int textRenderType: Text.QtRendering
    readonly property int textRenderQuality: Text.HighRenderTypeQuality
}
