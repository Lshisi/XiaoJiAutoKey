import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import XiaoJi.UI

QuietCard {
    id: root
    required property QtObject page
    readonly property var pageState: page.state
    glass: true
    horizontalPadding: Theme.controlCardPadding
    implicitHeight: body.implicitHeight + topPadding + bottomPadding
    ColumnLayout {
        id: body
        anchors.fill: parent
        spacing: 8
        RowLayout {
            id: headingRow
            Layout.fillWidth: true
            spacing: 6
            QuietHeading { id: heading; Layout.fillWidth: true; text: qsTr("快捷控制") }
            Loader {
                active: Theme.crispyThemeEnabled && headingRow.width >= heading.implicitWidth + 22
                visible: active
                Layout.preferredWidth: 16
                Layout.preferredHeight: 16
                sourceComponent: QuietIcon { name: "drumstick"; pixelSize: 16; color: Theme.crispyGold; preserveColors: true }
            }
        }
        ActionButton {
            id: runButton
            readonly property string actionText: root.pageState.running ? qsTr("停止") : qsTr("启动")
            readonly property bool hasHotkey: root.pageState.hotkey.length > 0
                && root.pageState.hotkey !== qsTr("未设置") && root.pageState.hotkey !== qsTr("录制中")
            Layout.fillWidth: true
            implicitHeight: Theme.primaryHeight
            primary: true
            destructive: root.pageState.running
            enabled: root.pageState.running || (root.pageState.profileEnabled && !onboardingModel.state.startBlocked)
            text: {
                const key = root.pageState.hotkey
                return hasHotkey ? qsTr("%1 (%2)").arg(actionText).arg(key) : actionText
            }
            hint: text
            contentItem: RowLayout {
                spacing: 6
                Item { Layout.fillWidth: true }
                QuietIcon {
                    name: root.pageState.running ? "stop" : "play"
                    color: runButton.foregroundColor
                    pixelSize: Theme.iconGlyphSize
                    Layout.preferredWidth: Theme.iconGlyphSize
                    Layout.preferredHeight: Theme.iconGlyphSize
                }
                QuietLabel {
                    text: runButton.actionText
                    color: runButton.foregroundColor
                    font: runButton.font
                    wrapMode: Text.NoWrap
                }
                QuietLabel {
                    visible: runButton.hasHotkey
                    Layout.minimumWidth: 0
                    Layout.maximumWidth: 58
                    text: root.pageState.hotkey
                    color: runButton.foregroundColor
                    font.pixelSize: Theme.captionFontSize
                    leftPadding: 7
                    elide: Text.ElideRight
                    wrapMode: Text.NoWrap
                    Rectangle {
                        anchors.left: parent.left
                        anchors.verticalCenter: parent.verticalCenter
                        width: 1
                        height: 12
                        color: Qt.alpha(runButton.foregroundColor, 0.4)
                    }
                }
                Item { Layout.fillWidth: true }
            }
            onClicked: root.page.startStopClicked()
            Accessible.name: root.pageState.running ? qsTr("停止连发") : qsTr("启动连发")
        }
    }
}
