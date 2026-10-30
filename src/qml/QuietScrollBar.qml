import QtQuick
import QtQuick.Templates as T
import XiaoJi.UI

T.ScrollBar {
    id: control
    property bool autoHide: false
    property bool recentlyScrolled: false
    implicitWidth: 8
    implicitHeight: 8
    padding: 1
    hoverEnabled: true
    minimumSize: 0.06
    visible: policy !== T.ScrollBar.AlwaysOff && size > 0 && size < 1
    onPositionChanged: {
        if (autoHide && visible) {
            recentlyScrolled = true
            hideDelay.restart()
        }
    }
    onHoveredChanged: {
        if (!hovered && recentlyScrolled)
            hideDelay.restart()
    }
    onPressedChanged: {
        if (!pressed && recentlyScrolled)
            hideDelay.restart()
    }
    onVisibleChanged: {
        if (!visible) {
            recentlyScrolled = false
            hideDelay.stop()
        }
    }
    Timer {
        id: hideDelay
        interval: 1000
        onTriggered: {
            if (!control.hovered && !control.pressed)
                control.recentlyScrolled = false
        }
    }
    contentItem: Rectangle {
        implicitWidth: 6
        implicitHeight: 6
        radius: 3
        color: control.hovered || control.pressed ? Theme.colors.scrollHandleHover : Theme.colors.scrollHandle
        opacity: !control.autoHide || control.recentlyScrolled || control.pressed ? 1 : 0
        visible: control.policy !== T.ScrollBar.AlwaysOff && control.size > 0 && control.size < 1
    }
    background: Item {}
}
