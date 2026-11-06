pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import XiaoJi.UI

Item {
    id: root
    property var model: dialogModel
    readonly property var overview: model.dialogState.overview
    implicitHeight: keyboardBody.implicitHeight
    onWidthChanged: detailsPopup.close()

    function showKey(keyId: string, anchor: Item) {
        model.edit("selectedKey", keyId)
        detailsPopup.showFor(anchor)
    }

    ColumnLayout {
        id: keyboardBody
        width: root.width
        spacing: 10
        KeyOverviewKeyboard {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            Layout.preferredHeight: implicitHeight
            overview: root.overview
            detailsOpen: detailsPopup.visible
            onKeyActivated: (keyId, anchor) => root.showKey(keyId, anchor)
            onMovementStarted: detailsPopup.close()
        }
        Flow {
            Layout.fillWidth: true
            spacing: 6
            QuietLabel {
                height: 42
                text: qsTr("鼠标")
                verticalAlignment: Text.AlignVCenter
                color: Theme.muted
                rightPadding: 6
            }
            Repeater {
                model: root.overview.mouse
                delegate: KeyOverviewKey {
                    id: mouseKey
                    required property var modelData
                    keyData: modelData
                    usage: root.overview.keyStates[modelData.id]
                    selected: detailsPopup.visible && root.overview.selectedId === modelData.id
                    width: 76
                    onClicked: root.showKey(modelData.id, mouseKey)
                }
            }
        }
        Flow {
            visible: root.overview.otherKeys.length > 0
            Layout.fillWidth: true
            spacing: 6
            Repeater {
                model: root.overview.otherKeys
                delegate: KeyOverviewKey {
                    id: otherKey
                    required property var modelData
                    keyData: modelData
                    usage: root.overview.keyStates[modelData.id]
                    selected: detailsPopup.visible && root.overview.selectedId === modelData.id
                    width: 104
                    onClicked: root.showKey(modelData.id, otherKey)
                }
            }
        }
        Flow {
            id: legend
            Layout.fillWidth: true
            spacing: Theme.cardGap
            Repeater {
                model: ["output", "hotkey", "pause", "sequence", "modifier"]
                delegate: Row {
                    id: roleLegend
                    required property string modelData
                    readonly property var role: Theme.keyOverviewRoles[modelData]
                    spacing: 5
                    Rectangle {
                        anchors.verticalCenter: parent.verticalCenter
                        width: 8
                        height: 8
                        radius: 2
                        color: roleLegend.role.color
                        Accessible.ignored: true
                    }
                    QuietLabel { text: roleLegend.role.label; color: Theme.muted; font.pixelSize: Theme.captionFontSize }
                }
            }
            QuietLabel {
                width: Math.min(implicitWidth, legend.width)
                text: root.overview.running ? qsTr("%1 · 仅查看").arg(root.overview.status) : root.overview.status
                color: Theme.muted
                font.pixelSize: Theme.captionFontSize
            }
        }
    }
    KeyOverviewPopup { id: detailsPopup; overview: root.overview; model: root.model }
}
