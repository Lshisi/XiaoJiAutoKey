import QtQuick
import QtQuick.Layouts
import XiaoJi.UI

RowLayout {
    id: root
    required property QtObject pluginApi
    spacing: Theme.cardGap
    PetSession { id: session; pluginApi: root.pluginApi }
    PetSkin {
        id: skin
        requestedId: root.pluginApi.settings.followFaction === true ? root.pluginApi.theme.factionId : root.pluginApi.settings.skinId || "cangjian"
    }
    PetSprite {
        id: character
        Layout.preferredWidth: 128
        Layout.preferredHeight: 128
        skin: skin.current
        action: session.action
        active: root.visible && Window.window !== null && Window.window.visible && Window.window.visibility !== Window.Minimized
        motionEnabled: Theme.motionEnabled
    }
    ColumnLayout {
        Layout.fillWidth: true
        spacing: 8
        QuietHeading { text: qsTr("%1小叽").arg(skin.current.name); Layout.fillWidth: true }
        QuietLabel {
            Layout.fillWidth: true
            text: session.running ? session.statusText || qsTr("陪你待着") : qsTr("准备好陪你闯江湖了")
        }
        QuietLabel {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            color: character.failed ? Theme.accent : Theme.muted
            text: character.failed ? qsTr("皮肤素材加载失败，请重新安装桌宠插件。")
                : qsTr("点击摸头，拖动摆放。练功时陪你挥剑，停下来就一起收工。")
        }
        RowLayout {
            Layout.fillWidth: true
            ActionButton {
                text: session.running ? qsTr("关闭桌宠") : qsTr("开启桌宠")
                enabled: root.pluginApi.runtime.status !== "starting" && root.pluginApi.runtime.status !== "stopping"
                onClicked: root.pluginApi.setBackgroundRunning(!session.running)
            }
            ActionButton {
                text: qsTr("摸摸头")
                quiet: true
                enabled: session.running
                onClicked: root.pluginApi.request("interact")
            }
        }
    }
}
