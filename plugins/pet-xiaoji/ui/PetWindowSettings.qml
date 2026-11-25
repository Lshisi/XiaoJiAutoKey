import QtQuick
import QtQuick.Layouts
import XiaoJi.UI

ColumnLayout {
    id: root
    required property QtObject pluginApi
    readonly property bool running: root.pluginApi.runtime.backgroundRunning === true
    readonly property var petWindow: {
        const windows = root.pluginApi.runtime.windows || []
        for (let i = 0; i < windows.length; ++i)
            if (windows[i].id === "pet") return windows[i]
        return { open: false, visible: false, topmost: true, clickThrough: false }
    }
    readonly property var sizes: [160, 224, 288]
    spacing: 12
    RowLayout {
        Layout.fillWidth: true
        QuietLabel { text: qsTr("桌宠大小"); Layout.fillWidth: true }
        QuietCombo {
            Layout.preferredWidth: 160
            model: [qsTr("小"), qsTr("中"), qsTr("大")]
            currentIndex: Math.max(0, root.sizes.indexOf(root.pluginApi.settings.size || 224))
            Accessible.name: qsTr("桌宠大小")
            onActivated: root.pluginApi.saveSetting("size", root.sizes[currentIndex])
        }
    }
    RowLayout {
        Layout.fillWidth: true
        QuietLabel { text: qsTr("窗口置顶"); Layout.fillWidth: true }
        QuietSwitch {
            enabled: root.petWindow.open
            checked: root.petWindow.topmost === true
            Accessible.name: qsTr("窗口置顶")
            onToggled: root.pluginApi.configureWindow("pet", { topmost: checked })
        }
    }
    RowLayout {
        Layout.fillWidth: true
        QuietLabel { text: qsTr("鼠标穿透"); Layout.fillWidth: true }
        QuietSwitch {
            enabled: root.petWindow.open
            checked: root.petWindow.clickThrough === true
            Accessible.name: qsTr("鼠标穿透")
            onToggled: root.pluginApi.configureWindow("pet", { clickThrough: checked })
        }
    }
    RowLayout {
        Layout.fillWidth: true
        ActionButton {
            text: root.petWindow.visible ? qsTr("隐藏桌宠") : qsTr("显示桌宠")
            quiet: true
            enabled: root.running
            onClicked: {
                if (root.petWindow.visible) root.pluginApi.hideWindow("pet")
                else root.pluginApi.showWindow("pet")
            }
        }
        ActionButton {
            text: qsTr("找回桌宠")
            quiet: true
            enabled: root.running
            onClicked: {
                root.pluginApi.showWindow("pet")
                root.pluginApi.configureWindow("pet", { resetPosition: true, clickThrough: false })
            }
        }
    }
    QuietLabel {
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
        color: Theme.muted
        text: qsTr("穿透后可从这里找回并恢复交互。开启一次后，下次随小叽恢复；关闭桌宠会取消自动恢复。")
    }
}
