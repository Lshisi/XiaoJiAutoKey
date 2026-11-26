import QtQuick
import QtQuick.Controls.Basic
import XiaoJi.UI

TextField {
    id: control
    CrispyControlFeedback { targetControl: control }
    implicitHeight: Math.max(32, contentHeight + 14)
    font.family: Theme.fontFamily
    font.pixelSize: Theme.bodyFontSize
    font.weight: Theme.bodyWeight
    font.hintingPreference: Theme.fontHintingPreference
    renderType: Theme.textRenderType
    color: Theme.bodyText
    placeholderTextColor: Theme.muted
    selectionColor: Theme.accent
    selectedTextColor: Theme.accentForeground
    selectByMouse: true
    hoverEnabled: true
    leftPadding: 10
    rightPadding: 10
    background: Rectangle {
        radius: Theme.inputRadius
        color: Theme.field
        border.color: control.activeFocus ? Theme.accent : control.enabled && control.hovered ? Theme.accentBorder : Theme.border
        border.width: control.activeFocus ? 2 : 1
    }
    TextEditMenu { id: contextMenu; textControl: control; onModified: control.textEdited() }
    Keys.onPressed: event => {
        if (event.key === Qt.Key_Menu || (event.key === Qt.Key_F10 && (event.modifiers & Qt.ShiftModifier))) {
            contextMenu.openAt(Qt.point(0, control.height));
            event.accepted = true;
        }
    }
}
