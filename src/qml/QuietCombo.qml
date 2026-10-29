import QtQuick
import QtQuick.Controls.Basic
import XiaoJi.UI

ComboBox {
    id: control
    implicitHeight: Theme.buttonHeight
    implicitWidth: 160
    leftPadding: 10
    rightPadding: 28
    font.family: Theme.fontFamily
    font.pixelSize: 12
    hoverEnabled: true
    contentItem: Text {
        text: control.displayText
        textFormat: Text.PlainText
        font: control.font
        color: control.enabled ? Theme.text : Theme.muted
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
    indicator: Text {
        x: control.width - width - 10
        y: (control.height - height) / 2
        text: "\uE70D"
        font.family: Theme.iconFamily
        font.pixelSize: 10
        color: Theme.muted
        Accessible.ignored: true
    }
    background: Rectangle {
        radius: Theme.inputRadius
        color: control.down ? Theme.hover : Theme.field
        border.color: control.visualFocus ? Theme.accent : Theme.border
        border.width: control.visualFocus ? 2 : 1
    }
    delegate: ItemDelegate {
        id: optionDelegate
        required property int index
        width: control.width - 8
        text: control.textAt(index)
        highlighted: control.highlightedIndex === index
        font: control.font
        contentItem: Text {
            text: optionDelegate.text
            textFormat: Text.PlainText
            color: Theme.text
            font: control.font
            elide: Text.ElideRight
            verticalAlignment: Text.AlignVCenter
        }
        background: Rectangle { radius: 6; color: optionDelegate.highlighted ? Theme.accentSoft : "transparent" }
    }
    popup: Popup {
        y: control.height + 4
        width: control.width
        height: Math.min(contentItem.implicitHeight + topPadding + bottomPadding, 300)
        margins: 8
        padding: 4
        contentItem: ListView {
            clip: true
            implicitHeight: contentHeight
            model: control.popup.visible ? control.delegateModel : null
            currentIndex: control.highlightedIndex
            ScrollBar.vertical: QuietScrollBar {}
        }
        background: Rectangle { color: Theme.surface; radius: 8; border.color: Theme.border }
    }
}
