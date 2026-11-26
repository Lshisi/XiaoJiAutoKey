import QtQuick
import QtQuick.Controls.Basic
import XiaoJi.UI

ActionButton {
    id: sticker
    readonly property bool playful: visible && Theme.crispyThemeEnabled && Theme.motionEnabled
    implicitWidth: 100
    implicitHeight: 104
    quiet: true
    leftPadding: 0
    rightPadding: 0
    topPadding: 0
    bottomPadding: 0
    hint: qsTr("戳一戳炸鸡桶")
    onPressed: { if (playful) bucketAnimation.restart() }
    onPlayfulChanged: {
        if (!playful) {
            bucketAnimation.stop()
            bucket.rotation = 0
            bucket.scale = 1
        }
    }
    HoverHandler { cursorShape: Qt.PointingHandCursor }
    contentItem: Item {
        FactionSymbol {
            id: bucket
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 6
            width: 82
            height: 90
            transformOrigin: Item.Bottom
            iconSource: "qrc:/assets/themes/crispy-bucket.svg"
            tintColor: Theme.crispyGold
            preserveColors: true
        }
    }
    background: Rectangle {
        visible: sticker.visualFocus
        color: "transparent"
        radius: Theme.buttonRadius
        border.width: 1
        border.color: Theme.accent
    }
    ParallelAnimation {
        id: bucketAnimation
        SequentialAnimation {
            RotationAnimator { target: bucket; from: 0; to: -7; duration: 60; easing.type: Easing.OutCubic }
            RotationAnimator { target: bucket; from: -7; to: 6; duration: 90; easing.type: Easing.OutCubic }
            RotationAnimator { target: bucket; from: 6; to: -3; duration: 80; easing.type: Easing.OutCubic }
            RotationAnimator { target: bucket; from: -3; to: 0; duration: 70; easing.type: Easing.OutCubic }
        }
        SequentialAnimation {
            ScaleAnimator { target: bucket; from: 1; to: 1.06; duration: 70; easing.type: Easing.OutCubic }
            ScaleAnimator { target: bucket; from: 1.06; to: 1; duration: 230; easing.type: Easing.OutBack }
        }
    }
}
