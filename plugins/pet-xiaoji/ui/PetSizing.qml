import QtQuick

QtObject {
    id: root
    property var storedSize
    readonly property var sizes: [72, 96, 120]
    readonly property var legacySizes: [160, 224, 288]
    readonly property int selectedIndex: {
        const value = Number(root.storedSize)
        const current = root.sizes.indexOf(value)
        if (current >= 0) return current
        const legacy = root.legacySizes.indexOf(value)
        return legacy >= 0 ? legacy : 1
    }
    readonly property int size: root.sizes[root.selectedIndex]
}
