import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import XiaoJi.UI

IconButton {
    id: root

    required property QtObject page
    required property string stepId
    property bool actionsEnabled: true

    iconName: "more"
    iconPixelSize: 16
    hint: qsTr("更多步骤操作")
    enabled: root.actionsEnabled
    onClicked: morePopup.open()

    Popup {
        id: morePopup
        y: root.height + 4
        width: 112
        height: actions.implicitHeight + topPadding + bottomPadding
        padding: 4
        margins: 8
        contentItem: ColumnLayout {
            id: actions
            spacing: 2
            QuietMenuItem {
                Layout.fillWidth: true
                text: qsTr("在前面插入")
                onTriggered: {
                    morePopup.close()
                    root.page.insertStepBeforeRequested(root.stepId)
                }
            }
            QuietMenuItem {
                Layout.fillWidth: true
                text: qsTr("在后面插入")
                onTriggered: {
                    morePopup.close()
                    root.page.insertStepAfterRequested(root.stepId)
                }
            }
            QuietMenuItem {
                Layout.fillWidth: true
                text: qsTr("复制此步骤")
                onTriggered: {
                    morePopup.close()
                    root.page.duplicateStepRequested(root.stepId)
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
