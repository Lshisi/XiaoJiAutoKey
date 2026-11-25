import QtQuick
import QtQuick.Layouts
import XiaoJi.UI

RowLayout {
    spacing: 8
    Rectangle {
        Layout.preferredWidth: 30
        Layout.preferredHeight: 30
        Layout.alignment: Qt.AlignVCenter
        radius: 7
        color: Qt.tint(Theme.crispyPaper, Qt.alpha(Theme.crispyAccent, 0.08))
        QuietIcon { anchors.centerIn: parent; name: "sparkles"; color: Theme.crispyAccent; pixelSize: 16 }
        Accessible.ignored: true
    }
    ColumnLayout {
        Layout.fillWidth: true
        Layout.minimumWidth: 0
        spacing: 2
        CrispyReceiptLabel { Layout.fillWidth: true; text: qsTr("已解锁「藏剑 · 脆皮山庄」"); font.weight: Font.DemiBold }
        CrispyReceiptLabel { Layout.fillWidth: true; text: qsTr("永久收藏，随时换装。"); color: Theme.crispyMuted; font.pixelSize: Theme.captionFontSize }
    }
}
