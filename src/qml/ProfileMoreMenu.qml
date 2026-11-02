import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import XiaoJi.UI

IconButton {
    id: root
    required property QtObject page
    required property bool macroMode
    property bool actionsEnabled: true

    iconName: "more"
    iconPixelSize: 16
    hint: qsTr("更多配置操作")
    onClicked: morePopup.open()

    Popup {
        id: morePopup
        y: root.height + 4
        width: 104
        height: actions.implicitHeight + topPadding + bottomPadding
        padding: 4
        margins: 8
        enter: Transition {
            enabled: Theme.motionEnabled
            OpacityAnimator { from: 0.8; to: 1; duration: Theme.popupEnterDuration; easing.type: Easing.OutCubic }
        }
        exit: Transition {
            enabled: Theme.motionEnabled
            OpacityAnimator { from: 1; to: 0; duration: Theme.popupExitDuration }
        }
        contentItem: ColumnLayout {
            id: actions
            spacing: 2
            QuietMenuItem {
                Layout.fillWidth: true
                text: qsTr("重命名配置")
                enabled: root.actionsEnabled
                onTriggered: {
                    morePopup.close()
                    if (root.macroMode) root.page.macroRenameRequested()
                    else root.page.profileRenameRequested()
                }
            }
            Item {
                Layout.fillWidth: true
                implicitHeight: 7
                Accessible.ignored: true
                Rectangle {
                    anchors.centerIn: parent
                    width: parent.width - 8
                    height: 1
                    color: Theme.divider
                }
            }
            QuietMenuItem {
                Layout.fillWidth: true
                text: qsTr("删除配置")
                enabled: root.actionsEnabled
                onTriggered: {
                    morePopup.close()
                    if (root.macroMode) root.page.macroDeleteRequested()
                    else root.page.profileDeleteRequested()
                }
            }
        }
        background: Rectangle {
            color: Theme.surface
            radius: Theme.inputRadius
            border.color: Theme.border
        }
    }
}
