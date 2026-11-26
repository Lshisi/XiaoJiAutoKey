import QtQuick
import XiaoJi.UI

Column {
    id: sticker
    width: 100
    spacing: 6
    Accessible.ignored: true
    FactionSymbol {
        anchors.horizontalCenter: parent.horizontalCenter
        width: 88
        height: 96
        iconSource: "qrc:/assets/themes/crispy-bucket.svg"
        tintColor: Theme.crispyGold
        preserveColors: true
    }
    QuietLabel {
        width: sticker.width
        text: qsTr("疯狂星期四")
        color: Theme.crispyAccent
        font.pixelSize: Theme.bodyFontSize
        font.letterSpacing: 1
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.NoWrap
        Accessible.ignored: true
    }
}
