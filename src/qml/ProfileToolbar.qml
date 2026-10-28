import QtQuick
import QtQuick.Layouts
import XiaoJi.UI

GridLayout {
    id: root
    required property QtObject page
    property bool macroMode: false
    columns: width < 450 ? 1 : 2
    columnSpacing: 7
    rowSpacing: 6
    RowLayout {
        Layout.fillWidth: true
        spacing: 7
        QuietLabel { text: qsTr("按键配置"); color: Theme.muted; font.pixelSize: 12 }
        QuietCombo {
            Layout.fillWidth: true
            Layout.minimumWidth: 90
            model: root.page.state.profiles
            textRole: "name"
            valueRole: "id"
            currentIndex: root.page.state.profileIndex
            onActivated: root.page.selectProfile(currentValue)
            Accessible.name: qsTr("选择配置")
        }
        QuietCheck {
            text: qsTr("启用")
            checked: root.page.state.profileEnabled
            onClicked: root.macroMode ? root.page.macroEnabledToggled(checked) : root.page.profileEnabledToggled(checked)
        }
    }
    RowLayout {
        Layout.alignment: Qt.AlignRight
        spacing: 1
        IconButton { glyph: "\uE710"; hint: qsTr("新增配置"); onClicked: root.macroMode ? root.page.macroAddRequested() : root.page.profileAddRequested() }
        IconButton { glyph: "\uE70F"; hint: qsTr("重命名配置"); onClicked: root.macroMode ? root.page.macroRenameRequested() : root.page.profileRenameRequested() }
        IconButton { glyph: "\uE74D"; hint: qsTr("删除配置"); onClicked: root.macroMode ? root.page.macroDeleteRequested() : root.page.profileDeleteRequested() }
        IconButton { glyph: "\uE72D"; hint: qsTr("分享配置"); onClicked: root.page.shareRequested() }
    }
}
