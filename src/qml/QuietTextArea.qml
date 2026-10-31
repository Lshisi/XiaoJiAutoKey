import QtQuick
import QtQuick.Controls.Basic
import XiaoJi.UI

QuietScrollView {
    id: control
    property alias text: editor.text
    property alias placeholderText: editor.placeholderText
    property alias readOnly: editor.readOnly
    signal edited(string value)
    implicitHeight: 180
    contentWidth: availableWidth
    background: Rectangle { color: Theme.field; radius: Theme.inputRadius; border.color: editor.activeFocus ? Theme.accent : editor.enabled && editor.hovered ? Theme.accentBorder : Theme.border }
    TextArea {
        id: editor
        textFormat: TextEdit.PlainText
        font.family: Theme.fontFamily
        font.pixelSize: Theme.bodyFontSize
        renderType: Theme.textRenderType
        color: Theme.text
        placeholderTextColor: Theme.muted
        selectionColor: Theme.accent
        selectedTextColor: Theme.accentForeground
        selectByMouse: true
        hoverEnabled: true
        wrapMode: TextEdit.WrapAnywhere
        padding: 12
        onTextChanged: { if (activeFocus) control.edited(text) }
        background: Item {}
        TextEditMenu { id: contextMenu; textControl: editor; onModified: control.edited(editor.text) }
        Keys.onPressed: event => {
            if (event.key === Qt.Key_Menu || (event.key === Qt.Key_F10 && (event.modifiers & Qt.ShiftModifier))) {
                contextMenu.openAt(Qt.point(0, editor.cursorRectangle.y + editor.cursorRectangle.height));
                event.accepted = true;
            }
        }
    }
}
