pragma ComponentBehavior: Bound
import QtQuick
import XiaoJi.UI

Item {
    id: burst
    property bool small: true
    readonly property int duration: small ? 250 : 310
    readonly property bool playing: Theme.crispyThemeEnabled && Theme.motionEnabled
    readonly property var offsets: small
        ? [Qt.point(-10, -9), Qt.point(2, -14), Qt.point(12, -4), Qt.point(3, 9)]
        : [Qt.point(-17, -14), Qt.point(-4, -22), Qt.point(12, -19), Qt.point(20, -5), Qt.point(9, 13)]
    width: 64
    height: 64
    z: 1000000
    enabled: false
    Accessible.ignored: true
    onPlayingChanged: { if (!playing) destroy() }
    Timer { interval: burst.duration; running: true; onTriggered: burst.destroy() }
    Repeater {
        model: burst.offsets
        delegate: Rectangle {
            id: crumb
            required property int index
            required property point modelData
            width: burst.small ? 3 : 4
            height: width
            radius: 1
            color: index % 3 === 2 ? Theme.crispyGold : Qt.lighter(Theme.crispyGold, 1.4)
            ParallelAnimation {
                running: burst.playing && burst.visible
                XAnimator { target: crumb; from: 30; to: 30 + crumb.modelData.x; duration: burst.duration; easing.type: Easing.OutCubic }
                YAnimator { target: crumb; from: 30; to: 30 + crumb.modelData.y; duration: burst.duration; easing.type: Easing.OutCubic }
                RotationAnimator { target: crumb; from: 0; to: 90 + crumb.index * 31; duration: burst.duration }
                SequentialAnimation {
                    PauseAnimation { duration: 60 }
                    OpacityAnimator { target: crumb; from: 0.95; to: 0; duration: burst.duration - 60 }
                }
            }
        }
    }
}
