import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import XiaoJi.UI

Button {
    id: banner
    implicitHeight: Math.max(52, contentItem.implicitHeight + topPadding + bottomPadding)
    leftPadding: 14
    rightPadding: 8
    topPadding: 8
    bottomPadding: 8
    hoverEnabled: true
    focusPolicy: Qt.TabFocus
    Accessible.name: qsTr("检测到分享配置，点击导入")
    onClicked: shareBannerModel.importShare()
    background: Rectangle {
        radius: Theme.cardRadius
        color: banner.down ? Theme.accentSoft : banner.hovered ? Theme.accentSubtle : Theme.surface
        border.color: banner.visualFocus || banner.down ? Theme.accent : Theme.border
        border.width: banner.visualFocus ? 2 : 1
    }
    contentItem: RowLayout {
        spacing: 8
        Text { text: "\uE896"; font.family: Theme.iconFamily; font.pixelSize: 16; color: Theme.accent; Accessible.ignored: true }
        QuietLabel { Layout.fillWidth: true; Layout.minimumWidth: 0; text: qsTr("检测到剪贴板中的分享配置"); elide: Text.ElideRight; wrapMode: Text.NoWrap }
        QuietLabel { text: qsTr("点击导入"); color: Theme.accent }
        IconButton { glyph: "\uE711"; hint: qsTr("关闭分享提示"); onClicked: shareBannerModel.dismiss() }
    }
}
