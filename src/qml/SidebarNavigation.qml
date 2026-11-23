import QtQuick
import QtQuick.Controls
import XiaoJi.UI

ListView {
    id: navigation
    property Item draggedEntry: null
    property real dragStartContentY: 0
    property real dragPointerX: 0
    property real dragPointerY: 0
    property int dropIndex: -1
    model: sidebarModel
    spacing: 2
    clip: true
    boundsBehavior: Flickable.StopAtBounds
    interactive: !draggedEntry
    onCountChanged: Qt.callLater(navigation.revealCurrentPage)
    ScrollBar.vertical: QuietScrollBar { autoHide: true }

    function revealCurrentPage() {
        currentIndex = sidebarModel.indexOfPage(appView.currentPage)
        if (currentIndex >= 0)
            positionViewAtIndex(currentIndex, ListView.Contain)
    }

    function updateDropIndex() {
        if (!draggedEntry || dragPointerX < 0 || dragPointerX > width
                || dragPointerY < 0 || dragPointerY > height) {
            dropIndex = -1
            return
        }
        const rowSize = draggedEntry.height + spacing
        const startY = draggedEntry.y - draggedEntry.index * rowSize
        dropIndex = Math.max(0, Math.min(count - 1, Math.floor((contentY + dragPointerY - startY) / rowSize)))
    }

    function updateDragPosition(scenePosition) {
        const position = mapFromItem(null, scenePosition.x, scenePosition.y)
        dragPointerX = position.x
        dragPointerY = position.y
        updateDropIndex()
    }

    Connections {
        target: appView
        function onCurrentPageChanged() { Qt.callLater(navigation.revealCurrentPage) }
    }

    Connections {
        target: sidebarModel
        function onModelAboutToBeReset() {
            navigation.draggedEntry = null
            navigation.dropIndex = -1
        }
    }

    Timer {
        interval: 16
        repeat: true
        running: navigation.visible && navigation.draggedEntry !== null && Qt.application.state === Qt.ApplicationActive
        onTriggered: {
            if (navigation.dragPointerX < 0 || navigation.dragPointerX > navigation.width
                    || navigation.dragPointerY < 0 || navigation.dragPointerY > navigation.height) return
            const edge = Math.min(32, navigation.height / 4)
            const step = navigation.dragPointerY < edge ? -6
                : navigation.dragPointerY > navigation.height - edge ? 6 : 0
            if (!step) return
            navigation.contentY = Math.max(navigation.originY,
                Math.min(navigation.originY + Math.max(0, navigation.contentHeight - navigation.height), navigation.contentY + step))
            navigation.updateDropIndex()
        }
    }

    delegate: SidebarEntry {
        id: entry
        required property var model
        required property int index
        property real dragOffset: 0
        x: 6
        width: navigation.width - 12
        pageId: model.pageId
        text: model.title
        iconName: model.icon
        dragging: dragHandler.active
        z: dragging ? 2 : 0
        transform: Translate {
            y: entry.dragging ? entry.dragOffset + navigation.contentY - navigation.dragStartContentY : 0
        }
        Keys.onPressed: event => {
            if (!(event.modifiers & Qt.AltModifier)) return
            const offset = event.key === Qt.Key_Up ? -1 : event.key === Qt.Key_Down ? 1 : 0
            if (!offset) return
            const targetIndex = Math.max(0, Math.min(navigation.count - 1, entry.index + offset))
            sidebarModel.movePage(entry.pageId, targetIndex)
            navigation.currentIndex = targetIndex
            navigation.positionViewAtIndex(targetIndex, ListView.Contain)
            event.accepted = true
        }
        Rectangle {
            visible: navigation.draggedEntry !== null && navigation.draggedEntry !== entry
                && navigation.dropIndex === entry.index
            anchors.left: parent.left
            anchors.right: parent.right
            y: navigation.draggedEntry && entry.index > navigation.draggedEntry.index ? entry.height : -height
            height: 2
            radius: 1
            color: Theme.accent
        }
        DragHandler {
            id: dragHandler
            target: null
            xAxis.enabled: false
            grabPermissions: PointerHandler.CanTakeOverFromAnything
            onActiveTranslationChanged: {
                if (!active) return
                entry.dragOffset = activeTranslation.y
            }
            onCentroidChanged: {
                if (active) navigation.updateDragPosition(centroid.scenePosition)
            }
            onActiveChanged: {
                if (active) {
                    entry.forceActiveFocus()
                    navigation.currentIndex = entry.index
                    entry.dragOffset = 0
                    navigation.dragStartContentY = navigation.contentY
                    navigation.draggedEntry = entry
                    navigation.updateDragPosition(centroid.scenePosition)
                }
            }
            onGrabChanged: (transition, point) => {
                if (navigation.draggedEntry !== entry
                        || (transition !== PointerDevice.UngrabExclusive && transition !== PointerDevice.CancelGrabExclusive)) return
                if (transition === PointerDevice.UngrabExclusive)
                    navigation.updateDragPosition(point.scenePosition)
                const targetIndex = navigation.dropIndex
                navigation.draggedEntry = null
                navigation.dropIndex = -1
                entry.dragOffset = 0
                if (transition === PointerDevice.UngrabExclusive)
                    sidebarModel.movePage(entry.pageId, targetIndex)
            }
        }
    }
}
