pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Templates as T
import XiaoJi.UI

TapHandler {
    id: feedback
    required property Item targetControl
    property bool small: true
    readonly property T.AbstractButton button: targetControl as T.AbstractButton
    readonly property T.ComboBox combo: targetControl as T.ComboBox
    readonly property bool feedbackEnabled: Theme.crispyThemeEnabled && Theme.motionEnabled
    readonly property point activationPosition: pressed
        ? point.position : Qt.point(targetControl.width / 2, targetControl.height / 2)

    parent: targetControl
    enabled: feedbackEnabled && targetControl.enabled && targetControl.visible
    acceptedButtons: Qt.LeftButton
    gesturePolicy: TapHandler.DragThreshold

    onPressedChanged: {
        if (pressed && !feedback.button && !feedback.combo)
            CrispyEffects.burst(feedback.targetControl, point.position, feedback.small)
    }
    readonly property Connections buttonConnection: Connections {
        target: feedback.button
        enabled: feedback.enabled
        function onPressed() {
            CrispyEffects.burst(feedback.targetControl, feedback.activationPosition, feedback.small)
        }
    }
    readonly property Connections comboConnection: Connections {
        target: feedback.combo
        enabled: feedback.enabled
        function onPressedChanged() {
            if (feedback.combo.pressed)
                CrispyEffects.burst(feedback.targetControl, feedback.activationPosition, feedback.small)
        }
    }
}
