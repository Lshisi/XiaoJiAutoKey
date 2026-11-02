import QtQuick
import QtQuick.Layouts
import XiaoJi.UI

RowLayout {
    id: root
    required property QtObject page
    property bool macroMode: false
    readonly property bool expandedLabels: width >= 400
    spacing: Theme.cardGap
    RowLayout {
        Layout.fillWidth: true
        spacing: 6
        QuietLabel {
            text: root.expandedLabels
                ? (root.macroMode ? qsTr("循环配置") : qsTr("按键配置"))
                : qsTr("配置")
            color: Theme.muted
            wrapMode: Text.NoWrap
        }
        QuietCombo {
            Layout.fillWidth: true
            Layout.minimumWidth: 64
            model: root.page.state.profiles
            textRole: "name"
            valueRole: "id"
            currentIndex: root.page.state.profileIndex
            enabled: !root.page.state.running
            onActivated: root.page.selectProfile(currentValue)
            Accessible.name: root.macroMode ? qsTr("选择循环配置") : qsTr("选择按键配置")
        }
    }
    RowLayout {
        Layout.fillWidth: false
        spacing: 6
        QuietLabel { text: root.expandedLabels ? qsTr("启用配置") : qsTr("启用"); color: Theme.muted; wrapMode: Text.NoWrap }
        QuietSwitch {
            checked: root.page.state.profileEnabled
            onClicked: root.macroMode ? root.page.macroEnabledToggled(checked) : root.page.profileEnabledToggled(checked)
            Accessible.name: qsTr("启用配置")
        }
    }
    RowLayout {
        Layout.fillWidth: false
        spacing: 2
        IconButton { iconName: "add"; hint: qsTr("新增配置"); enabled: !root.page.state.running; onClicked: root.macroMode ? root.page.macroAddRequested() : root.page.profileAddRequested() }
        IconButton { iconName: "rename"; hint: qsTr("重命名配置"); enabled: !root.page.state.running; onClicked: root.macroMode ? root.page.macroRenameRequested() : root.page.profileRenameRequested() }
        IconButton { Layout.leftMargin: 4; iconName: "delete"; hint: qsTr("删除配置"); enabled: !root.page.state.running; onClicked: root.macroMode ? root.page.macroDeleteRequested() : root.page.profileDeleteRequested() }
        IconButton {
            iconName: "export"
            hint: root.macroMode ? qsTr("导出当前宏") : qsTr("导出当前方案")
            onClicked: root.page.shareRequested()
        }
    }
}
