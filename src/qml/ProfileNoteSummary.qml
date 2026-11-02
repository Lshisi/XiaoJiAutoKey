import QtQuick
import QtQuick.Layouts
import XiaoJi.UI

ActionButton {
    id: root
    required property QtObject page
    readonly property string note: page.state.note
    visible: note.length > 0
    enabled: !page.state.running
    quiet: true
    leftPadding: 0
    rightPadding: 0
    topPadding: 0
    bottomPadding: 0
    implicitHeight: contentItem.implicitHeight
    hint: qsTr("点击查看或编辑完整备注")
    Accessible.name: qsTr("编辑配置备注")
    Accessible.description: note
    onClicked: page.noteEditRequested()
    contentItem: QuietLabel {
        text: qsTr("备注：%1").arg(root.note.replace(/[\r\n]+/g, " "))
        color: Theme.muted
        wrapMode: Text.NoWrap
        elide: Text.ElideRight
    }
}
