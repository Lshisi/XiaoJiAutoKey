pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import XiaoJi.UI

Item {
    id: shell
    property var model: dialogModel
    readonly property var dialogData: model.dialogState
    readonly property bool overviewPage: dialogData.page === "DialogKeyOverview.qml"
    readonly property var pageLoader: overviewPage ? overviewContent : content
    readonly property real preferredWidth: (pageLoader.status === Loader.Ready && pageLoader.item
        && pageLoader.item.preferredWidth !== undefined ? pageLoader.item.preferredWidth : pageLoader.implicitWidth)
        + 2 * Theme.dialogPadding
    readonly property real preferredHeight: header.implicitHeight + pageLoader.implicitHeight + footer.implicitHeight
        + 2 * Theme.dialogVerticalPadding + (footer.visible ? 2 : 1) * Theme.cardGap
    Rectangle {
        anchors.fill: parent
        radius: Theme.dialogRadius
        color: Theme.surface
        border.color: Theme.border
        border.width: 1
        Accessible.ignored: true
    }
    Loader {
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 4
        width: 20
        height: 20
        z: 1
        active: shell.dialogData.resizable === true
        sourceComponent: Item {
            QuietIcon { anchors.fill: parent; name: "resize"; color: Theme.muted; pixelSize: 16 }
            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.SizeFDiagCursor
                onPressed: shell.model.startWindowResize(Qt.RightEdge | Qt.BottomEdge)
                Accessible.ignored: true
            }
        }
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
                MouseArea { anchors.fill: parent; onPressed: shell.model.startWindowMove(); Accessible.ignored: true }
            }
            CaptionButton {
                objectName: "dialogCancel"
                closeAction: true
                enabled: shell.dialogData.closeAllowed
                onClicked: shell.model.dispatch("cancel")
            }
        }
        QuietScrollView {
            id: scroll
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentWidth: availableWidth
            Item {
                width: scroll.availableWidth
                height: Math.max(implicitHeight, scroll.availableHeight)
                implicitHeight: shell.pageLoader.implicitHeight
                Loader {
                    id: content
                    anchors.fill: parent
                    active: !shell.overviewPage
                    source: active ? shell.dialogData.page : ""
                }
                Loader {
                    id: overviewContent
                    anchors.fill: parent
                    active: shell.overviewPage
                    sourceComponent: DialogKeyOverview { model: shell.model }
                }
            }
        }
        Flow {
            id: footer
            visible: shell.dialogData.actions.length > 0
            Layout.fillWidth: true
            layoutDirection: Qt.RightToLeft
            spacing: 8
            Repeater {
                model: shell.dialogData.actions
                delegate: ActionButton {
                    required property var modelData
                    objectName: "dialogAction_" + modelData.id
                    text: modelData.text
                    primary: modelData.primary
                    destructive: modelData.danger
                    enabled: modelData.enabled
                    onClicked: shell.model.dispatch(modelData.id)
                }
            }
        }
    }
}
