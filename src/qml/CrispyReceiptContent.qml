pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import XiaoJi.UI

ColumnLayout {
    id: content
    property bool animateEntrance: false
    spacing: 8
    CrispyReceiptLabel {
        Layout.fillWidth: true
        Layout.minimumHeight: Theme.compactIconSize
        text: qsTr("江湖隐藏菜单 · 周四限定")
        color: Theme.crispyMuted
        font.pixelSize: Theme.captionFontSize
        font.letterSpacing: 1
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }
    CrispyReceiptLabel {
        Layout.fillWidth: true
        text: qsTr("脆皮山庄 · 周四出餐单")
        font.pixelSize: Theme.pageTitleFontSize - 1
        font.weight: Font.DemiBold
        horizontalAlignment: Text.AlignHCenter
    }
    CrispyReceiptLabel {
        Layout.fillWidth: true
        text: qsTr("今日小叽，外酥里嫩。")
        horizontalAlignment: Text.AlignHCenter
        color: Theme.crispyMuted
        font.pixelSize: Theme.captionFontSize
    }
    Item {
        Layout.fillWidth: true
        Layout.preferredHeight: 86
        FactionSymbol {
            id: mascot
            anchors.centerIn: parent
            width: 84
            height: 84
            iconSource: "qrc:/assets/factions/cangjian-crispy.svg"
            tintColor: Theme.crispyGold
            preserveColors: true
        }
        Rectangle {
            anchors.right: parent.right
            anchors.rightMargin: 8
            anchors.verticalCenter: parent.verticalCenter
            width: 42
            height: 42
            radius: 5
            rotation: 12
            color: "transparent"
            border.width: 2
            border.color: Theme.crispyAccent
            Accessible.ignored: true
            CrispyReceiptLabel { anchors.centerIn: parent; width: 27; text: qsTr("已出\n餐"); color: Theme.crispyAccent; horizontalAlignment: Text.AlignHCenter; font.letterSpacing: 1 }
        }
        ParallelAnimation {
            running: content.animateEntrance && content.visible
            ScaleAnimator { target: mascot; from: 0.84; to: 1; duration: 240; easing.type: Easing.OutBack }
            RotationAnimator { target: mascot; from: -8; to: 0; duration: 240; easing.type: Easing.OutCubic }
        }
    }
    CrispyReceiptDivider { Layout.fillWidth: true }
    RowLayout {
        Layout.fillWidth: true
        CrispyReceiptLabel { text: qsTr("ORDER · CJ-050"); color: Theme.crispyMuted; font.family: Theme.crispyMonoFontFamily; font.pixelSize: Theme.captionFontSize }
        Item { Layout.fillWidth: true }
        CrispyReceiptLabel { text: qsTr("星期四"); color: Theme.crispyMuted; font.pixelSize: Theme.captionFontSize }
    }
    Repeater {
        model: [qsTr("招牌脆皮小叽"), qsTr("随身轻重双剑")]
        delegate: RowLayout {
            id: orderLine
            required property string modelData
            Layout.fillWidth: true
            CrispyReceiptLabel { Layout.fillWidth: true; text: orderLine.modelData; font.weight: Font.DemiBold }
            CrispyReceiptLabel { text: qsTr("× 1") }
        }
    }
    CrispyReceiptLabel { Layout.fillWidth: true; text: qsTr("备注：轻剑重剑分开装。"); color: Theme.crispyMuted; font.pixelSize: Theme.captionFontSize }
    CrispyReceiptDivider { Layout.fillWidth: true }
    RowLayout {
        Layout.fillWidth: true
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2
            CrispyReceiptLabel { Layout.fillWidth: true; text: qsTr("江湖救急，赎回庄主。") }
            CrispyReceiptLabel { text: qsTr("本次营救暗号"); color: Theme.crispyMuted; font.pixelSize: Theme.captionFontSize }
        }
        CrispyReceiptLabel {
            text: qsTr("V50")
            color: Theme.crispyAccent
            font.family: Theme.crispyMonoFontFamily
            font.pixelSize: 32
            font.weight: Font.Bold
            font.letterSpacing: -1.5
        }
    }
}
