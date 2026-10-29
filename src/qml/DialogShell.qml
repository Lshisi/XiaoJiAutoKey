import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import XiaoJi.UI

Item {
    id: shell
    readonly property var dialogData: dialogModel.dialogState
    readonly property real preferredHeight: header.implicitHeight + content.implicitHeight + footer.implicitHeight
        + 2 * Theme.dialogVerticalPadding + 2 * Theme.cardGap
    Rectangle {
        anchors.fill: parent
        radius: Theme.dialogRadius
        color: Theme.surface
        border.color: Theme.border
        border.width: 1
        Accessible.ignored: true
    }
    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: Theme.dialogPadding
        anchors.rightMargin: Theme.dialogPadding
        anchors.topMargin: Theme.dialogVerticalPadding
        anchors.bottomMargin: Theme.dialogVerticalPadding
        spacing: Theme.cardGap
        RowLayout {
            id: header
            Layout.fillWidth: true
            Item {
                Layout.fillWidth: true
                implicitHeight: 32
                QuietHeading { anchors.fill: parent; text: shell.dialogData.title; verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight; wrapMode: Text.NoWrap }
                MouseArea { anchors.fill: parent; onPressed: dialogModel.startWindowMove(); Accessible.ignored: true }
            }
            IconButton {
                glyph: "\u00D7"
                font.family: Theme.fontFamily
                font.pixelSize: 13
                hint: qsTr("关闭")
                enabled: shell.dialogData.closeAllowed
                onClicked: dialogModel.dispatch("cancel")
            }
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
                height: Math.max(implicitHeight, scroll.availableHeight)
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
