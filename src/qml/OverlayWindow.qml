import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Item {
    id: overlay
    readonly property var appearance: overlayModel.overlayState
    implicitWidth: 224
    implicitHeight: body.implicitHeight + 2 * (appearance.glassInset + appearance.contentPadding)
    LiquidGlass {
        anchors.fill: parent
        dark: overlay.appearance.dark
        surfaceColor: overlay.appearance.glassColor
        materialOpacity: overlay.appearance.opacity / 100
        shadowInset: overlay.appearance.glassInset
        lightPoint: overlayModel.lightPosition
    }
    ColumnLayout {
        id: body
        anchors.fill: parent
        anchors.margins: overlay.appearance.glassInset + overlay.appearance.contentPadding
        spacing: 10
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
        Rectangle {
            visible: overlay.appearance.showRepeat && overlay.appearance.showMacro
            Layout.fillWidth: true
            implicitHeight: 1
            color: Qt.alpha(overlay.appearance.muted, 0.18)
            Accessible.ignored: true
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
                font.pixelSize: overlay.appearance.fontPixelSize - 1
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
