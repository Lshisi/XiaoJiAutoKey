pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import XiaoJi.UI

GridLayout {
    id: sound
    readonly property var fields: dialogModel.dialogState
    readonly property var cue: fields.events.find(event => event.id === fields.selectedEvent)
    readonly property bool groupEnabled: fields.group === 0 ? fields.repeatGroupEnabled : fields.macroGroupEnabled
    readonly property bool stacked: width < 440
    columns: sound.stacked ? 1 : 3
    columnSpacing: Theme.cardGap
    rowSpacing: Theme.cardGap

    SoundEventList {
        id: eventList
        Layout.preferredWidth: sound.stacked ? -1 : Math.max(Theme.sidebarWidth, eventList.implicitWidth)
        Layout.minimumWidth: 0
        Layout.fillWidth: sound.stacked
        Layout.alignment: Qt.AlignTop
        events: sound.fields.events
        selectedEvent: sound.fields.selectedEvent
        onEventSelected: eventId => dialogModel.edit("selectedEvent", eventId)
    }
    Rectangle {
        Layout.preferredWidth: 1
        Layout.fillHeight: true
        visible: !sound.stacked
        color: Theme.divider
        Accessible.ignored: true
    }
    ColumnLayout {
        Layout.fillWidth: true
        Layout.minimumWidth: 0
        Layout.alignment: Qt.AlignTop
        spacing: Theme.cardGap

        QuietHeading { Layout.fillWidth: true; text: sound.cue.title }
        QuietLabel {
            Layout.fillWidth: true
            Layout.minimumHeight: Theme.buttonHeight
            text: sound.cue.description
            color: Theme.muted
        }
        QuietCheck {
            text: qsTr("启用此项提示音")
            checked: sound.cue.enabled
            onClicked: dialogModel.edit("cueEnabled", checked)
            Accessible.name: qsTr("启用%1提示音").arg(sound.cue.title)
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            QuietLabel { text: qsTr("音量") }
            QuietSlider {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                value: sound.cue.volume
                onMoved: dialogModel.edit("cueVolume", Math.round(value))
                Accessible.name: qsTr("%1提示音音量").arg(sound.cue.title)
            }
            QuietLabel { text: qsTr("%1%").arg(sound.cue.volume); color: Theme.muted; Layout.minimumWidth: 34 }
        }
        QuietLabel { text: qsTr("音频文件"); color: Theme.muted }
        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            ActionButton {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                text: sound.cue.customPath.length ? sound.cue.customPath : qsTr("内置：%1").arg(sound.cue.defaultFile)
                textAlignment: Text.AlignLeft
                hint: sound.cue.customPath.length ? sound.cue.customPath : qsTr("选择自定义音频，默认文件为 %1").arg(sound.cue.defaultFile)
                onClicked: dialogModel.dispatch("chooseSound")
                Accessible.name: qsTr("选择%1音频文件").arg(sound.cue.title)
            }
            IconButton {
                iconName: "play"
                hint: qsTr("试听%1，试听不受开关影响").arg(sound.cue.title)
                enabled: sound.cue.available && sound.cue.volume > 0
                onClicked: dialogModel.dispatch("previewSound")
            }
            IconButton {
                iconName: "refresh"
                hint: qsTr("恢复默认音频：%1").arg(sound.cue.defaultFile)
                enabled: sound.cue.customPath.length > 0
                onClicked: dialogModel.dispatch("resetSound")
            }
        }
        QuietLabel {
            Layout.fillWidth: true
            visible: !sound.cue.available
            text: sound.cue.customPath.length ? qsTr("自定义音频不存在，请重新选择。")
                : qsTr("待放入默认音频：audio/%1").arg(sound.cue.defaultFile)
            color: Theme.muted
        }
        QuietLabel {
            Layout.fillWidth: true
            visible: !sound.groupEnabled
            text: sound.fields.group === 0 ? qsTr("按键页的整组提示音已关闭，单项设置仍可保存和试听。")
                : qsTr("循环页的整组提示音已关闭，单项设置仍可保存和试听。")
            color: Theme.muted
        }
        QuietLabel {
            Layout.fillWidth: true
            visible: sound.fields.status.length > 0
            text: sound.fields.status
            color: Theme.muted
        }
        QuietLabel {
            Layout.fillWidth: true
            text: qsTr("每项分别保存开关、音量和音频。支持 mp3、wav、wma、mid、midi。")
            color: Theme.muted
        }
    }
}
