import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import XiaoJi.UI

ActionButton {
    id: root
    property bool sidebar: false
    readonly property int popupWidth: {
        let width = 112
        for (let i = 0; i < appView.factions.length; ++i) {
            width = Math.max(width, popupMetrics.advanceWidth(appView.factions[i].name) + 62)
        }
        return width
    }
    implicitHeight: sidebar ? 58 : Theme.buttonHeight
    quiet: sidebar
    text: Theme.colors.factionName
    hint: qsTr("切换门派主题")
    onClicked: factionPopup.open()
    FontMetrics {
        id: popupMetrics
        font.family: Theme.fontFamily
        font.pixelSize: 12
    }
    contentItem: RowLayout {
        spacing: 7
        Item {
            visible: root.sidebar
            Layout.preferredWidth: 34
            Layout.preferredHeight: 34
            FactionSymbol {
                anchors.fill: parent
                iconSource: Theme.colors.factionIcon
                tintColor: Theme.text
            }
        }
        QuietLabel {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            text: root.text
            font.weight: root.sidebar ? Theme.headingWeight : Font.Normal
            elide: Text.ElideRight
            wrapMode: Text.NoWrap
        }
        QuietIcon {
            visible: !root.sidebar
            name: "chevron-down"
            pixelSize: Theme.smallIconGlyphSize
            color: Theme.muted
        }
    }
    Popup {
        id: factionPopup
        y: root.height + 4
        width: Math.max(112, root.popupWidth)
        height: Math.min(factionList.contentHeight + 8, 512)
        padding: 4
        margins: 8
        enter: Transition {
            enabled: Theme.motionEnabled
            OpacityAnimator { from: 0.8; to: 1; duration: Theme.popupEnterDuration; easing.type: Easing.OutCubic }
        }
        exit: Transition {
            enabled: Theme.motionEnabled
            OpacityAnimator { from: 1; to: 0; duration: Theme.popupExitDuration }
        }
        contentItem: ListView {
            id: factionList
            clip: true
            model: appView.factions
            delegate: ItemDelegate {
                id: option
                required property var modelData
                width: factionList.width
                implicitHeight: Math.max(24, factionName.implicitHeight + topPadding + bottomPadding)
                leftPadding: 6
                rightPadding: 6
                topPadding: 3
                bottomPadding: 3
                text: modelData.name
                highlighted: modelData.id === Theme.colors.factionId
                hoverEnabled: true
                onClicked: { appView.selectFaction(modelData.id); factionPopup.close() }
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
                    Item {
                        Layout.preferredWidth: 16
                        Layout.preferredHeight: 16
                        Layout.minimumWidth: 16
                        Layout.maximumWidth: 16
                        Layout.alignment: Qt.AlignVCenter
                        FactionSymbol {
                            anchors.fill: parent
                            iconSource: option.modelData.icon
                            tintColor: option.highlighted ? Theme.accent : Theme.text
                        }
                    }
                    QuietLabel {
                        id: factionName
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        text: option.text
                        color: option.highlighted ? Theme.accent : Theme.text
                        elide: Text.ElideRight
                        wrapMode: Text.NoWrap
                    }
                }
                background: Rectangle { radius: Theme.buttonRadius; color: option.highlighted || option.down ? Theme.accentSoft : option.hovered ? Theme.accentSubtle : "transparent" }
            }
            ScrollBar.vertical: QuietScrollBar {}
        }
        background: Rectangle { color: Theme.surface; radius: Theme.inputRadius; border.color: Theme.border }
    }
}
