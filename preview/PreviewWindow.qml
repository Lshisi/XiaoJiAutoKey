import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import XiaoJi.UI

Window {
    id: preview
    width: 780
    height: 600
    minimumWidth: 780
    minimumHeight: 600
    maximumHeight: 740
    flags: Qt.Window | Qt.FramelessWindowHint
    visible: true
    title: qsTr("小叽按键 · 全界面预览")
    color: "transparent"

    ColumnLayout {
        anchors.fill: parent
        spacing: 0
        AppShell {
            id: appShell
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 560
        }
        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: Theme.pageMargin
            Layout.rightMargin: Theme.pageMargin
            Layout.minimumHeight: 40
            spacing: Theme.cardGap
            QuietLabel {
                Layout.fillWidth: true
                text: qsTr("界面预览 · 操作仅修改示例数据")
                color: Theme.muted
                font.pixelSize: Theme.captionFontSize
            }
            QuietCheck { text: qsTr("深色"); checked: Theme.dark; onToggled: appView.setDarkMode(checked) }
            QuietCheck { text: qsTr("示例数据"); checked: appView.demoData; onToggled: appView.setDemoData(checked) }
            QuietCombo {
                Layout.preferredWidth: 132
                model: [qsTr("其它窗口预览…"), qsTr("关闭确认"), qsTr("导入配置"), qsTr("更新下载"),
                    qsTr("分享提示"), qsTr("新手引导"), qsTr("坐标拾取")]
                onActivated: { appView.showExample(currentIndex); currentIndex = 0 }
                Accessible.name: qsTr("选择其它窗口预览")
            }
        }
    }
    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 40
        color: Theme.surfaceTint
        z: -1
        Accessible.ignored: true
    }
    Loader {
        active: dialogModel.visible
        sourceComponent: PreviewDialogWindow {
            ownerWindow: preview
            anchorItem: appShell.workspaceDialogAnchor
        }
    }
    Loader {
        active: appView.overlayVisible
        sourceComponent: PreviewOverlayWindow { ownerWindow: preview }
    }
    Loader {
        active: appView.pickerVisible
        sourceComponent: PreviewPickerWindow { ownerWindow: preview }
    }
}
