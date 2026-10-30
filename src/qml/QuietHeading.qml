import QtQuick
import XiaoJi.UI

QuietLabel {
    property bool pageTitle: false
    font.pixelSize: pageTitle ? Theme.pageTitleFontSize : Theme.cardTitleFontSize
    font.weight: Theme.headingWeight
}
