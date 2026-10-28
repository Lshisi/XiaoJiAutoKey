import QtQuick
import XiaoJi.UI

QuietLabel {
    property bool pageTitle: false
    font.pixelSize: pageTitle ? 19 : 15
    font.weight: Font.DemiBold
}
