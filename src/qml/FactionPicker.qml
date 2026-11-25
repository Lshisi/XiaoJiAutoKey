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
        if (settingsPageModel.state.crispyThemeUnlocked)
            width = Math.max(width, popupMetrics.advanceWidth(qsTr("藏剑 · 脆皮山庄")) + 62)
        return width
    }
    implicitHeight: sidebar ? 58 : Theme.buttonHeight
    quiet: sidebar
    text: sidebar && Theme.colors.crispyThemeEnabled ? qsTr("脆皮山庄") : Theme.colors.themeName
    hint: qsTr("切换门派主题（%1）").arg(Theme.colors.themeName)
    onClicked: factionPopup.open()
    FontMetrics {
        id: popupMetrics
        font: root.font
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
                preserveColors: Theme.colors.crispyThemeEnabled
            }
        }
        ColumnLayout {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            spacing: 4
            QuietLabel {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                text: root.text
                color: root.sidebar && Theme.crispyThemeEnabled ? Theme.accent : Theme.bodyText
                font.weight: root.sidebar && Theme.crispyThemeEnabled ? Font.DemiBold : root.sidebar ? Theme.headingWeight : Theme.bodyWeight
                elide: Text.ElideRight
                wrapMode: Text.NoWrap
            }
            Loader {
                active: root.sidebar && Theme.crispyThemeEnabled
                visible: active
                Layout.preferredWidth: 30
                Layout.preferredHeight: 9
                sourceComponent: CrispyStripes {}
            }
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
            delegate: FactionMenuEntry {
                required property var modelData
                width: factionList.width
                factionName: modelData.name
                factionIcon: modelData.icon
                selected: modelData.id === Theme.colors.factionId && !Theme.colors.crispyThemeEnabled
                onClicked: { appView.selectFaction(modelData.id); factionPopup.close() }
            }
            footer: Column {
                width: factionList.width
                visible: settingsPageModel.state.crispyThemeUnlocked
                height: visible ? implicitHeight : 0
                spacing: 4
                Rectangle { width: parent.width; height: 1; color: Theme.divider }
                QuietLabel {
                    text: qsTr("隐藏外观")
                    leftPadding: 6
                    topPadding: 4
                    color: Theme.muted
                    font.pixelSize: Theme.captionFontSize
                }
                FactionMenuEntry {
                    width: factionList.width
                    factionName: qsTr("藏剑 · 脆皮山庄")
                    factionIcon: "qrc:/assets/factions/cangjian-crispy.svg"
                    preserveColors: true
                    selected: Theme.colors.crispyThemeEnabled
                    onClicked: {
                        factionPopup.close()
                        Qt.callLater(() => settingsPageModel.selectCrispyTheme())
                    }
                }
                ActionButton {
                    width: factionList.width
                    text: qsTr("查看出餐单")
                    quiet: true
                    compact: true
                    onClicked: {
                        factionPopup.close()
                        Qt.callLater(() => settingsPageModel.showCrispyThemeReceipt())
                    }
                }
            }
            ScrollBar.vertical: QuietScrollBar {}
        }
        background: Rectangle { color: Theme.surface; radius: Theme.inputRadius; border.color: Theme.border }
    }
}
