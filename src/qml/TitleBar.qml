import QtQuick
import QtQuick.Layouts
import XiaoJi.UI

Rectangle {
    id: titleBar
    implicitHeight: Math.max(Theme.titleBarHeight, content.implicitHeight + 12)
    color: Theme.background
    radius: 10
    bottomLeftRadius: 0
    bottomRightRadius: 0
    RowLayout {
        id: content
        anchors.fill: parent
        anchors.leftMargin: 12
        anchors.rightMargin: 8
        spacing: 4
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            implicitHeight: 32
            RowLayout {
                anchors.fill: parent
                spacing: 8
                Image {
                    Layout.preferredWidth: 20
                    Layout.preferredHeight: 20
                    source: appView.activityIcon
                    sourceSize: Qt.size(20 * Screen.devicePixelRatio, 20 * Screen.devicePixelRatio)
                    fillMode: Image.PreserveAspectFit
                    onStatusChanged: { if (status === Image.Error) console.error("Title icon failed:", source) }
                    Accessible.ignored: true
                }
                QuietLabel {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    text: appView.title
                    font.weight: Font.Bold
                    elide: Text.ElideRight
                    wrapMode: Text.NoWrap
                }
            }
            MouseArea {
                anchors.fill: parent
                acceptedButtons: Qt.LeftButton
                onPressed: appView.startWindowMove()
                onDoubleClicked: mouse => mouse.accepted = true
                Accessible.ignored: true
            }
        }
        CaptionButton { onClicked: appView.minimizeWindow() }
        CaptionButton { closeAction: true; onClicked: appView.closeWindow() }
    }
    Rectangle { anchors.bottom: parent.bottom; anchors.left: parent.left; anchors.right: parent.right; anchors.margins: 10; anchors.bottomMargin: 0; height: 1; color: Theme.border }
}
