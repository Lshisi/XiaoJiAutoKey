pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Templates as T
import XiaoJi.UI

TapHandler {
    id: feedback
    required property Item targetControl
    property bool small: true
    property bool pointerActivation: false
    property point pressPosition: Qt.point(0, 0)
    readonly property T.AbstractButton button: targetControl as T.AbstractButton
    readonly property T.ComboBox combo: targetControl as T.ComboBox
    readonly property bool feedbackEnabled: Theme.crispyThemeEnabled && Theme.motionEnabled
    readonly property point activationPosition: pointerActivation
        ? pressPosition : Qt.point(targetControl.width / 2, targetControl.height / 2)

    parent: targetControl
    enabled: feedbackEnabled && targetControl.enabled && targetControl.visible
    acceptedButtons: Qt.LeftButton
    gesturePolicy: TapHandler.DragThreshold

    function resetPointer() { if (!pressed) pointerActivation = false }

    onPressedChanged: {
        if (pressed) {
            pointerActivation = true
            pressPosition = point.position
        } else {
            Qt.callLater(feedback.resetPointer)
        }
    }
    onTapped: eventPoint => {
        if (!feedback.button && !feedback.combo)
            CrispyEffects.burst(feedback.targetControl, eventPoint.position, feedback.small)
    }
    readonly property Connections buttonConnection: Connections {
        target: feedback.button
        enabled: feedback.feedbackEnabled
        function onClicked() {
            CrispyEffects.burst(feedback.targetControl, feedback.activationPosition, feedback.small)
        }
    }
    readonly property Connections comboConnection: Connections {
        target: feedback.combo ? feedback.combo.popup : null
        enabled: feedback.feedbackEnabled
        function onAboutToShow() {
            CrispyEffects.burst(feedback.targetControl, feedback.activationPosition, feedback.small)
        }
    }
}
