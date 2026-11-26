import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import XiaoJi.UI

ItemDelegate {
    id: option
    CrispyControlFeedback { targetControl: option }
    required property string factionName
    required property url factionIcon
    property bool selected: false
    property bool preserveColors: false
    implicitHeight: Math.max(24, caption.implicitHeight + topPadding + bottomPadding)
    leftPadding: 6
    rightPadding: 6
    topPadding: 3
    bottomPadding: 3
    text: factionName
    highlighted: selected
    hoverEnabled: true
    contentItem: RowLayout {
        spacing: 6
        QuietIcon {
            Layout.preferredWidth: 12
            Layout.preferredHeight: 16
            Layout.alignment: Qt.AlignVCenter
            opacity: option.highlighted ? 1 : 0
            name: "check"
            pixelSize: Theme.smallIconGlyphSize
            color: Theme.accent
        }
        FactionSymbol {
            Layout.preferredWidth: 16
            Layout.preferredHeight: 16
            Layout.minimumWidth: 16
            Layout.maximumWidth: 16
            Layout.alignment: Qt.AlignVCenter
            iconSource: option.factionIcon
            tintColor: option.highlighted ? Theme.accent : Theme.text
            preserveColors: option.preserveColors
        }
        QuietLabel {
            id: caption
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            text: option.text
            color: option.highlighted ? Theme.accent : Theme.bodyText
            elide: Text.ElideRight
            wrapMode: Text.NoWrap
        }
    }
    background: Rectangle {
        radius: Theme.buttonRadius
        color: option.highlighted || option.down ? Theme.accentSoft : option.hovered ? Theme.accentSubtle : "transparent"
    }
}
