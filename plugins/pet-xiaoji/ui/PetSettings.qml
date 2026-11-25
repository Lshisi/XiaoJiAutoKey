import QtQuick
import QtQuick.Layouts
import XiaoJi.UI

ColumnLayout {
    id: root
    required property QtObject pluginApi
    spacing: 12
    PetSkin { id: skin; requestedId: root.pluginApi.settings.skinId || "cangjian" }
    RowLayout {
        Layout.fillWidth: true
        QuietLabel { text: qsTr("桌宠皮肤"); Layout.fillWidth: true }
        QuietCombo {
            Layout.preferredWidth: 160
            model: skin.available
            textRole: "name"
            enabled: root.pluginApi.settings.followFaction !== true
            currentIndex: {
                for (let i = 0; i < skin.available.length; ++i)
                    if (skin.available[i].id === skin.current.id) return i
                return 0
            }
            Accessible.name: qsTr("桌宠皮肤")
            onActivated: root.pluginApi.saveSetting("skinId", skin.available[currentIndex].id)
        }
    }
    RowLayout {
        Layout.fillWidth: true
        QuietLabel { text: qsTr("跟随主界面门派"); Layout.fillWidth: true }
        QuietSwitch {
            checked: root.pluginApi.settings.followFaction === true
            Accessible.name: qsTr("跟随主界面门派")
            onToggled: root.pluginApi.saveSetting("followFaction", checked)
        }
    }
    QuietLabel {
        Layout.fillWidth: true
        text: qsTr("首版提供藏剑皮肤。跟随其它门派时，暂时由藏剑小叽陪伴。")
        color: Theme.muted
        wrapMode: Text.WordWrap
    }
    RowLayout {
        Layout.fillWidth: true
        QuietLabel { text: qsTr("跟随运行状态练功"); Layout.fillWidth: true }
        QuietSwitch {
            checked: root.pluginApi.settings.linkRunning !== false
            Accessible.name: qsTr("跟随运行状态练功")
            onToggled: root.pluginApi.saveSetting("linkRunning", checked)
        }
    }
    QuietLabel {
        Layout.fillWidth: true
        text: qsTr("连发或循环运行时练剑，暂停时等候，全部停止后收工。闲下来两分钟会打盹。")
        color: Theme.muted
        wrapMode: Text.WordWrap
    }
}
