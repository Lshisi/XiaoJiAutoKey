import QtQuick

QtObject {
    id: root
    property string requestedId: "cangjian"
    readonly property var available: [
        { id: "cangjian", name: qsTr("藏剑"), source: Qt.resolvedUrl("../skins/cangjian/atlas.png"),
            columns: 4, rows: 6, frameWidth: 256, frameHeight: 256,
            actions: {
                idle: { row: 0, frame: 0, count: 4, duration: 100 },
                pet: { row: 1, frame: 0, count: 4, duration: 200 },
                drag: { row: 2, frame: 0, count: 2, duration: 220 },
                land: { row: 2, frame: 2, count: 2, duration: 180 },
                sleep: { row: 3, frame: 0, count: 4, duration: 650 },
                practice: { row: 4, frame: 0, count: 4, duration: 180 },
                rest: { row: 4, frame: 0, count: 1, duration: 1000 },
                finish: { row: 5, frame: 0, count: 4, duration: 220 }
            } }
    ]
    readonly property var current: {
        for (let i = 0; i < root.available.length; ++i)
            if (root.available[i].id === root.requestedId) return root.available[i]
        return root.available[0]
    }
    readonly property bool fallback: root.current.id !== root.requestedId
}
