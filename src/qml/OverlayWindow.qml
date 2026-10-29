import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Item {
    id: overlay
    readonly property var appearance: overlayModel.overlayState
    implicitWidth: 178 + 2 * appearance.glassInset
    implicitHeight: body.implicitHeight + 2 * appearance.glassInset + 17
    Rectangle {
        anchors.fill: parent
        radius: 6
        color: Qt.alpha(overlay.appearance.glassColor, overlay.appearance.opacity / 100)
        border.color: Qt.alpha(overlay.appearance.glassBorder,
            Math.min(1, overlay.appearance.opacity / 100 + 0.11))
        border.width: 1
        Accessible.ignored: true
    }
    ColumnLayout {
        id: body
        anchors.fill: parent
        anchors.leftMargin: overlay.appearance.glassInset + overlay.appearance.contentPadding
        anchors.rightMargin: overlay.appearance.glassInset + overlay.appearance.contentPadding
        anchors.topMargin: overlay.appearance.glassInset + 8
        anchors.bottomMargin: overlay.appearance.glassInset + 9
        spacing: 6
        Loader {
            Layout.fillWidth: true
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
            Layout.fillWidth: true
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
            Layout.fillWidth: true
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
