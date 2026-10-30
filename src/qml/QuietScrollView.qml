import QtQuick
import QtQuick.Templates as T
import XiaoJi.UI

// Use the template so Basic's default scroll bars are never created.
T.ScrollView {
    id: control
    property bool autoHideScrollBar: false
    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            contentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             contentHeight + topPadding + bottomPadding)
    clip: true
    contentWidth: availableWidth
    T.ScrollBar.vertical: QuietScrollBar {
        parent: control
        autoHide: control.autoHideScrollBar
        x: control.mirrored ? 0 : control.width - width
        y: control.topPadding
        height: control.availableHeight
    }
}
