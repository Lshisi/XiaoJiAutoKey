import QtQuick
import QtQuick.Controls.Basic

Item {
    id: overlay
    readonly property var appearance: overlayModel.overlayState
    implicitWidth: 178
    implicitHeight: Math.max(32, body.implicitHeight)
        + 2 * appearance.contentPadding
    Rectangle {
        anchors.fill: parent
        color: Qt.alpha(overlay.appearance.panelColor, overlay.appearance.opacity / 100)
        Accessible.ignored: true
    }
    Column {
        id: body
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.leftMargin: overlay.appearance.contentPadding
        anchors.rightMargin: overlay.appearance.contentPadding
        anchors.topMargin: 8
        spacing: 6
        Loader {
            width: body.width
            active: overlay.appearance.showRepeat
            visible: active
            sourceComponent: OverlayStatusBlock {
                appearance: overlay.appearance
                heading: overlay.appearance.repeatTitle
                lines: overlay.appearance.repeatLines
                active: overlay.appearance.repeatActive
                reserveLock: true
            }
        }
        Loader {
            width: body.width
            active: overlay.appearance.showMacro
            visible: active
            sourceComponent: OverlayStatusBlock {
                appearance: overlay.appearance
                heading: overlay.appearance.macroTitle
                lines: overlay.appearance.macroLines
                active: overlay.appearance.macroActive
                reserveLock: !overlay.appearance.showRepeat
            }
        }
        Loader {
            width: body.width
            active: overlay.appearance.gameGatePaused
            visible: active
            sourceComponent: Label {
                text: qsTr("非游戏窗口 · 已暂停")
                textFormat: Text.PlainText
                wrapMode: Text.Wrap
                font.family: overlay.appearance.fontFamily
                font.pixelSize: overlay.appearance.fontPixelSize
                color: overlay.appearance.warning
                padding: 6
                background: Rectangle {
                    radius: 6
                    color: Qt.alpha(overlay.appearance.warning, 0.12)
                }
            }
        }
    }
}
