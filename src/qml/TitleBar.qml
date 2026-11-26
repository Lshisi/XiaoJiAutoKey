import QtQuick
import QtQuick.Layouts
import XiaoJi.UI

Rectangle {
    id: titleBar
    readonly property int stripeHeight: Theme.crispyThemeEnabled ? 2 : 0
    implicitHeight: Math.max(Theme.titleBarHeight, content.implicitHeight + Math.max(2, stripeHeight))
    color: Theme.titleBarBackground
    radius: 10
    bottomLeftRadius: 0
    bottomRightRadius: 0
    RowLayout {
        id: content
        anchors.fill: parent
        anchors.leftMargin: 12
        anchors.rightMargin: 8
        anchors.bottomMargin: titleBar.stripeHeight
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
                    font.weight: Theme.headingWeight
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
    Rectangle {
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: 10
        anchors.rightMargin: 10
        height: 1
        visible: !Theme.crispyThemeEnabled
        color: Theme.border
        Accessible.ignored: true
    }
    Loader {
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: 10
        anchors.rightMargin: 10
        height: titleBar.stripeHeight
        active: Theme.crispyThemeEnabled
        visible: active
        sourceComponent: CrispyStripes {
            diagonal: true
            paperColor: Theme.dark ? Theme.titleBarBackground : Theme.crispyCream
        }
    }
}
