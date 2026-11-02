import QtQuick
import QtQuick.Layouts
import XiaoJi.UI

RowLayout {
    id: root
    required property QtObject page
    property bool macroMode: false
    readonly property bool expandedLabels: width >= 450
    spacing: 6
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
            Layout.minimumWidth: 110
            model: root.page.state.profiles
            textRole: "name"
            valueRole: "id"
            currentIndex: root.page.state.profileIndex
            enabled: !root.page.state.running
            onActivated: root.page.selectProfile(currentValue)
            Accessible.name: root.macroMode ? qsTr("选择循环配置") : qsTr("选择按键配置")
        }
        QuietLabel { text: root.expandedLabels ? qsTr("启用配置") : qsTr("启用"); color: Theme.muted; wrapMode: Text.NoWrap }
        QuietSwitch {
            checked: root.page.state.profileEnabled
            onClicked: root.macroMode ? root.page.macroEnabledToggled(checked) : root.page.profileEnabledToggled(checked)
            Accessible.name: qsTr("启用配置")
        }
    }
    RowLayout {
        Layout.fillWidth: false
        spacing: 4
        IconButton {
            iconName: "add"
            hint: root.macroMode ? qsTr("新增宏") : qsTr("新增方案")
            enabled: !root.page.state.running
            onClicked: root.macroMode ? root.page.macroAddRequested() : root.page.profileAddRequested()
        }
        IconButton {
            iconName: "copy"
            hint: root.macroMode ? qsTr("复制当前宏") : qsTr("复制当前方案")
            enabled: !root.page.state.running
            onClicked: root.macroMode ? root.page.macroDuplicateRequested() : root.page.profileDuplicateRequested()
        }
        IconButton {
            iconName: "export"
            hint: root.macroMode ? qsTr("导出当前宏") : qsTr("导出当前方案")
            onClicked: root.page.shareRequested()
        }
        ProfileMoreMenu {
            macroMode: root.macroMode
            page: root.page
            actionsEnabled: !root.page.state.running
        }
    }
}
