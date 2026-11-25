import QtQuick
import XiaoJi.UI

ActionButton {
    id: control
    font.family: Theme.crispyFontFamily
    font.weight: Font.DemiBold
    implicitHeight: Math.max(35, contentItem.implicitHeight + topPadding + bottomPadding)
    contentItem: CrispyReceiptLabel {
        text: control.text
        font: control.font
        color: !control.enabled ? Theme.muted : control.primary ? Theme.crispyAccentForeground : Theme.crispyText
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        wrapMode: Text.NoWrap
        elide: Text.ElideRight
    }
    background: Rectangle {
        radius: Theme.buttonRadius
        color: !control.enabled ? Theme.hover : control.primary
            ? (control.down ? Qt.darker(Theme.crispyBrand, 1.2) : control.hovered ? Qt.darker(Theme.crispyBrand, 1.1) : Theme.crispyBrand)
            : Qt.tint(Theme.crispyPaper, Qt.alpha(Theme.crispyAccent, control.down ? 0.12 : control.hovered ? 0.06 : 0))
        border.width: control.visualFocus ? 2 : control.primary ? 0 : 1
        border.color: control.visualFocus ? Theme.crispyAccent : Qt.alpha(Theme.crispyMuted, 0.28)
    }
}
