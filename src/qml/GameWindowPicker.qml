pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import XiaoJi.UI

QuietCombo {
    id: root
    required property var windows
    readonly property string query: search.text.trim().toLowerCase()
    readonly property var matches: windows.filter(entry => entry.title.toLowerCase().indexOf(query) >= 0
        || entry.processName.toLowerCase().indexOf(query) >= 0)
    readonly property var mainWindow: {
        let owner = root.Window.window
        while (owner && owner.transientParent) owner = owner.transientParent
        return owner
    }
    property real popupRoom: 260
    property bool popupAbove: false
    signal processSelected(string processName)
    signal refreshRequested()

    function fitPopup() {
        if (!mainWindow) return
        const position = root.mapToGlobal(0, 0)
        const below = Math.max(0, mainWindow.y + mainWindow.height - 8 - position.y - root.height - 4)
        const above = Math.max(0, position.y - mainWindow.y - Theme.titleBarHeight - 8 - 4)
        const preferred = Math.min(260, Math.max(100, matches.length * 56) + search.implicitHeight + 4 + 2 * root.popup.padding)
        popupAbove = below < preferred && above > below
        popupRoom = Math.floor(popupAbove ? above : below)
    }
    model: matches
    textRole: "title"
    currentIndex: -1
    displayText: qsTr("从已打开的窗口选择…")
    Accessible.name: displayText
    onActivated: index => {
        const entry = root.matches[index]
        if (entry && !entry.added) root.processSelected(entry.processName)
        currentIndex = -1
    }
    onVisibleChanged: { if (!visible) popup.close() }
    Connections {
        target: Qt.application
        function onStateChanged() {
            if (Qt.application.state !== Qt.ApplicationActive) root.popup.close()
        }
    }
    Connections {
        target: root.mainWindow
        enabled: root.popup.visible
        function onXChanged() { root.popup.close() }
        function onYChanged() { root.popup.close() }
        function onWidthChanged() { root.popup.close() }
        function onHeightChanged() { root.popup.close() }
        function onScreenChanged() { root.popup.close() }
    }

    delegate: ItemDelegate {
        id: entry
        required property int index
        required property var modelData
        width: root.popup.width - 2 * root.popup.padding - 10
        implicitHeight: Math.max(56, contentItem.implicitHeight + topPadding + bottomPadding)
        leftPadding: 8
        rightPadding: 8
        enabled: !modelData.added
        highlighted: root.highlightedIndex === index
        hoverEnabled: true
        Accessible.name: qsTr("%1，%2").arg(modelData.title).arg(modelData.processName)
        Accessible.description: modelData.builtIn ? qsTr("剑网3已内置识别")
            : modelData.added ? qsTr("该程序已添加") : qsTr("添加此程序的所有窗口")
        contentItem: RowLayout {
            spacing: 10
            GameWindowIcon { Layout.preferredWidth: 24; Layout.preferredHeight: 24; source: entry.modelData.icon }
            ColumnLayout {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                spacing: 2
                QuietLabel { Layout.fillWidth: true; text: entry.modelData.title; elide: Text.ElideRight; wrapMode: Text.NoWrap }
                QuietLabel { Layout.fillWidth: true; text: entry.modelData.processName; color: Theme.muted; font.pixelSize: Theme.captionFontSize; elide: Text.ElideRight; wrapMode: Text.NoWrap }
            }
            QuietLabel { text: entry.modelData.builtIn ? qsTr("内置") : entry.modelData.added ? qsTr("已添加") : qsTr("添加"); color: entry.modelData.added ? Theme.muted : Theme.accent }
        }
        background: Rectangle {
            radius: Theme.buttonRadius
            color: entry.down || entry.highlighted ? Theme.accentSoft : entry.hovered ? Theme.hover : "transparent"
            border.width: entry.visualFocus ? 2 : 0
            border.color: Theme.accent
        }
        QuietToolTip { text: qsTr("%1\n%2").arg(entry.modelData.title).arg(entry.modelData.processName); visible: entry.hovered }
    }
    popup: Popup {
        id: windowPopup
        popupType: Popup.Window
        y: root.popupAbove ? -windowPopup.height - 4 : root.height + 4
        width: root.width
        height: Math.min(contentItem.implicitHeight + topPadding + bottomPadding, 260, root.popupRoom)
        margins: 8
        padding: 4
        focus: true
        modal: false
        dim: false
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutsideParent
        onAboutToShow: { root.refreshRequested(); search.clear(); root.currentIndex = -1; root.fitPopup() }
        onOpened: search.forceActiveFocus()
        onActiveFocusChanged: { if (opened && !activeFocus) close() }
        contentItem: ColumnLayout {
            spacing: 4
            RowLayout {
                Layout.fillWidth: true
                QuietTextField {
                    id: search
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    placeholderText: qsTr("搜索窗口标题或程序名")
                    Accessible.name: placeholderText
                }
                IconButton { iconName: "refresh"; hint: qsTr("刷新窗口列表"); onClicked: root.refreshRequested() }
            }
            ListView {
                id: list
                Layout.fillWidth: true
                Layout.fillHeight: true
                implicitHeight: Math.max(100, contentHeight)
                clip: true
                reuseItems: true
                currentIndex: root.highlightedIndex
                model: root.popup.visible ? root.delegateModel : null
                ScrollBar.vertical: QuietScrollBar {}
                QuietLabel {
                    anchors.fill: parent
                    anchors.margins: Theme.cardGap
                    visible: list.count === 0
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    color: Theme.muted
                    text: root.windows.length ? qsTr("没有匹配的窗口") : qsTr("先打开目标程序，再刷新列表。也可以手动添加程序名。")
                }
            }
        }
        background: Rectangle { color: Theme.surface; radius: Theme.inputRadius; border.color: Theme.border }
    }
}
