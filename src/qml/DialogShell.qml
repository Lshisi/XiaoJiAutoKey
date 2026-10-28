import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import XiaoJi.UI

Item {
    id: shell
    readonly property var dialogData: dialogModel.dialogState
    readonly property real preferredHeight: header.implicitHeight + content.implicitHeight + footer.implicitHeight + 88
    Loader {
        anchors.fill: parent
        sourceComponent: shell.dialogData.hasBackdrop ? refractiveMaterial : plainMaterial
    }
    Component {
        id: refractiveMaterial
        DialogGlass {
            sourceUrl: dialogModel.backdropUrl
            sourceRegion: dialogModel.backdropRegion
        }
    }
    Component {
        id: plainMaterial
        LiquidGlass { dark: Theme.dark; surfaceColor: Theme.surface; materialOpacity: 0.97; cornerRadius: Theme.dialogRadius }
    }
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 28
        spacing: 16
        RowLayout {
            id: header
            Layout.fillWidth: true
            Item {
                Layout.fillWidth: true
                implicitHeight: 32
                QuietHeading { anchors.fill: parent; text: shell.dialogData.title; verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight; wrapMode: Text.NoWrap }
                MouseArea { anchors.fill: parent; onPressed: dialogModel.startWindowMove(); Accessible.ignored: true }
            }
            IconButton { glyph: "\uE711"; hint: qsTr("关闭"); enabled: shell.dialogData.closeAllowed; onClicked: dialogModel.dispatch("cancel") }
        }
        ScrollView {
            id: scroll
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentWidth: availableWidth
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
            ScrollBar.vertical: QuietScrollBar {}
            Loader {
                id: content
                width: scroll.availableWidth
                source: shell.dialogData.page
            }
        }
        Flow {
            id: footer
            Layout.fillWidth: true
            layoutDirection: Qt.RightToLeft
            spacing: 8
            Repeater {
                model: shell.dialogData.actions
                delegate: ActionButton {
                    required property var modelData
                    text: modelData.text
                    primary: modelData.primary
                    destructive: modelData.danger
                    enabled: modelData.enabled
                    onClicked: dialogModel.dispatch(modelData.id)
                }
            }
        }
    }
}
