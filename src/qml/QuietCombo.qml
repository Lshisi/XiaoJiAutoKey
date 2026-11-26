import QtQuick
import QtQuick.Controls.Basic
import XiaoJi.UI

ComboBox {
    id: control
    CrispyControlFeedback { targetControl: control }
    implicitHeight: Theme.buttonHeight
    implicitWidth: 160
    leftPadding: 10
    rightPadding: 28
    font.family: Theme.fontFamily
    font.pixelSize: Theme.bodyFontSize
    font.weight: Theme.bodyWeight
    font.hintingPreference: Theme.fontHintingPreference
    hoverEnabled: true
    contentItem: Text {
        text: control.displayText
        textFormat: Text.PlainText
        font: control.font
        renderType: Theme.textRenderType
        renderTypeQuality: Theme.textRenderQuality
        color: control.enabled ? Theme.bodyText : Theme.muted
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
    indicator: QuietIcon {
        x: control.width - width - 10
        y: (control.height - height) / 2
        name: "chevron-down"
        pixelSize: Theme.smallIconGlyphSize
        color: Theme.muted
        rotation: control.popup.visible ? 180 : 0
        Behavior on rotation {
            enabled: Theme.motionEnabled && control.visible
            RotationAnimator { duration: Theme.toggleDuration; easing.type: Easing.OutCubic }
        }
    }
    background: Rectangle {
        radius: Theme.inputRadius
        color: !control.enabled ? Theme.hover : control.down ? Theme.accentSoft : control.hovered ? Theme.accentSubtle : Theme.field
        border.color: control.visualFocus ? Theme.accent : control.enabled && control.hovered ? Theme.accentBorder : Theme.border
        border.width: control.visualFocus ? 2 : 1
    }
    delegate: ItemDelegate {
        id: optionDelegate
        CrispyControlFeedback { targetControl: optionDelegate }
        required property int index
        width: control.width - 8
        text: control.textAt(index)
        highlighted: control.highlightedIndex === index
        hoverEnabled: true
        font: control.font
        contentItem: Text {
            text: optionDelegate.text
            textFormat: Text.PlainText
            color: Theme.bodyText
            font: control.font
            renderType: Theme.textRenderType
            renderTypeQuality: Theme.textRenderQuality
            elide: Text.ElideRight
            verticalAlignment: Text.AlignVCenter
        }
        background: Rectangle { radius: Theme.buttonRadius; color: optionDelegate.highlighted || optionDelegate.down ? Theme.accentSoft : optionDelegate.hovered ? Theme.accentSubtle : "transparent" }
    }
    popup: Popup {
        y: control.height + 4
        width: control.width
        height: Math.min(contentItem.implicitHeight + topPadding + bottomPadding, 300)
        margins: 8
        padding: 4
        enter: Transition {
            enabled: Theme.motionEnabled
            OpacityAnimator { from: 0.8; to: 1; duration: Theme.popupEnterDuration; easing.type: Easing.OutCubic }
        }
        exit: Transition {
            enabled: Theme.motionEnabled
            OpacityAnimator { from: 1; to: 0; duration: Theme.popupExitDuration }
        }
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
