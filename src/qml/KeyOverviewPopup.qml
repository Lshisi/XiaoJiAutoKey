pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic
import XiaoJi.UI

Popup {
    id: popup
    required property var overview
    required property var model
    property Item anchorItem
    property point anchorTop
    property point anchorBottom
    parent: Overlay.overlay
    popupType: Popup.Item
    focus: true
    modal: false
    padding: Theme.cardPadding
    margins: 8
    width: Math.min(380, parent.width - 2 * margins)
    height: Math.min(detailsLoader.implicitHeight + 2 * padding, parent.height - 2 * margins)
    x: Math.max(margins, Math.min(anchorBottom.x - width / 2, parent.width - width - margins))
    y: anchorBottom.y + height + margins <= parent.height ? anchorBottom.y + 6
        : Math.max(margins, anchorTop.y - height - 6)
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    function showFor(anchor: Item) {
        anchorItem = anchor
        anchorTop = anchor.mapToItem(parent, anchor.width / 2, 0)
        anchorBottom = anchor.mapToItem(parent, anchor.width / 2, anchor.height)
        open()
    }

    onClosed: {
        if (anchorItem && anchorItem.visible) anchorItem.forceActiveFocus(Qt.PopupFocusReason)
    }
    contentItem: QuietScrollView {
        id: scroll
        Loader {
            id: detailsLoader
            width: scroll.availableWidth
            active: popup.visible
            sourceComponent: KeyOverviewDetails {
                overview: popup.overview
                onNavigationRequested: targetId => {
                    popup.model.dispatch("navigate", targetId)
                    popup.close()
                }
            }
        }
    }
    background: Rectangle { color: Theme.surface; radius: Theme.cardRadius; border.color: Theme.border }
}
