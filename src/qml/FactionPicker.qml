import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Effects
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
            Image {
                id: symbol
                anchors.fill: parent
                source: Theme.colors.factionIcon
                sourceSize: Qt.size(width * Screen.devicePixelRatio, height * Screen.devicePixelRatio)
                fillMode: Image.PreserveAspectFit
                visible: !Theme.dark
            }
            MultiEffect {
                anchors.fill: parent
                source: symbol
                colorization: 1
                colorizationColor: Theme.text
                visible: Theme.dark && symbol.status === Image.Ready
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
        Text {
            visible: !root.sidebar
            text: "\uE70D"
            font.family: Theme.iconFamily
            font.pixelSize: 10
            renderType: Text.NativeRendering
            color: Theme.muted
            Accessible.ignored: true
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
                    Text {
                        Layout.preferredWidth: 12
                        Layout.preferredHeight: 16
                        Layout.alignment: Qt.AlignVCenter
                        opacity: option.highlighted ? 1 : 0
                        text: "\uE73E"
                        font.family: Theme.iconFamily
                        font.pixelSize: 10
                        renderType: Text.NativeRendering
                        color: Theme.accent
                        verticalAlignment: Text.AlignVCenter
                        Accessible.ignored: true
                    }
                    Item {
                        Layout.preferredWidth: 16
                        Layout.preferredHeight: 16
                        Layout.minimumWidth: 16
                        Layout.maximumWidth: 16
                        Layout.alignment: Qt.AlignVCenter
                        Image {
                            id: optionSymbol
                            anchors.fill: parent
                            source: option.modelData.icon
                            sourceSize: Qt.size(16, 16)
                            fillMode: Image.PreserveAspectFit
                            visible: !Theme.dark
                            Accessible.ignored: true
                        }
                        MultiEffect {
                            anchors.fill: parent
                            source: optionSymbol
                            colorization: 1
                            colorizationColor: option.highlighted ? Theme.accent : Theme.text
                            visible: Theme.dark && optionSymbol.status === Image.Ready
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
