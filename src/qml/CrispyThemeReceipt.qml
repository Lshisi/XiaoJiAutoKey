pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import XiaoJi.UI

Popup {
    id: receipt
    readonly property QtObject page: settingsPageModel
    parent: Overlay.overlay
    popupType: Popup.Item
    anchors.centerIn: parent
    width: Math.min(344, parent.width - 2 * Theme.pageMargin)
    height: Math.min(implicitHeight, parent.height - 2 * Theme.pageMargin)
    contentHeight: sheet.implicitHeight
    margins: Theme.pageMargin
    padding: 0
    modal: true
    focus: true
    closePolicy: Popup.CloseOnEscape
    Component.onCompleted: open()
    onClosed: Qt.callLater(receipt.page.dismissCrispyThemeReceipt)
    Overlay.modal: Rectangle { color: Qt.rgba(0, 0, 0, Theme.dark ? 0.45 : 0.22) }
    enter: Transition {
        enabled: Theme.motionEnabled
        OpacityAnimator { from: 0; to: 1; duration: Theme.fadeDuration }
    }
    exit: Transition {
        enabled: Theme.motionEnabled
        OpacityAnimator { from: 1; to: 0; duration: Theme.popupExitDuration }
    }
    background: null
    contentItem: Item {
        id: sheet
        implicitHeight: ticketBody.implicitHeight + receiptFooter.implicitHeight + 10
            + Theme.dialogVerticalPadding + Theme.cardGap + paper.tearHeight
        Accessible.role: Accessible.Dialog
        Accessible.name: qsTr("脆皮山庄 · 周四出餐单")
        CrispyReceiptPaper {
            id: paper
            anchors.fill: parent
        }
        Item {
            anchors.fill: parent
            anchors.leftMargin: Theme.dialogPadding
            anchors.rightMargin: Theme.dialogPadding
            anchors.topMargin: Theme.dialogVerticalPadding
            anchors.bottomMargin: Theme.cardGap + paper.tearHeight
            QuietScrollView {
                id: scroll
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.bottom: receiptFooter.top
                anchors.bottomMargin: 10
                contentHeight: ticketBody.implicitHeight
                showScrollBar: false
                CrispyReceiptContent {
                    id: ticketBody
                    width: scroll.availableWidth
                    animateEntrance: receipt.visible && Theme.motionEnabled
                }
            }
            Column {
                id: receiptFooter
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                spacing: 10
                CrispyReceiptReward { width: receiptFooter.width }
                RowLayout {
                    width: receiptFooter.width
                    spacing: 8
                    uniformCellSizes: true
                    CrispyReceiptAction {
                        Layout.fillWidth: true
                        text: qsTr("立即换装")
                        primary: true
                        onClicked: Qt.callLater(receipt.page.selectCrispyTheme)
                    }
                    CrispyReceiptAction { Layout.fillWidth: true; text: qsTr("先收着"); onClicked: receipt.close() }
                }
                CrispyReceiptLabel {
                    width: receiptFooter.width
                    text: qsTr("谢谢惠顾，下周四见。")
                    horizontalAlignment: Text.AlignHCenter
                    color: Theme.crispyMuted
                    font.pixelSize: Theme.captionFontSize
                    wrapMode: Text.NoWrap
                }
            }
        }
    }
}
