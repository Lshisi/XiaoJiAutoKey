import QtQuick

QtObject {
    id: root
    property string requestedId: "cangjian"
    readonly property var available: [
        { id: "cangjian", name: qsTr("藏剑"), source: Qt.resolvedUrl("../skins/cangjian/atlas.png"),
            columns: 4, rows: 6, frameWidth: 256, frameHeight: 256,
            sheets: {
                practice: { source: Qt.resolvedUrl("../skins/cangjian/practice-pixel.png"),
                    columns: 6, rows: 4, frameWidth: 256, frameHeight: 256 },
                ambient: { source: Qt.resolvedUrl("../skins/cangjian/ambient-pixel.png"),
                    columns: 4, rows: 6, frameWidth: 256, frameHeight: 256 }
            },
            actions: {
                "idle": { "loopFrom": 0, "frames": [
                    { "row": 0, "column": 0, "duration": 3600 },
                    { "row": 0, "column": 1, "duration": 90 },
                    { "row": 0, "column": 2, "duration": 110 },
                    { "row": 0, "column": 3, "duration": 90 }
                ] },
                "pet": { "loopFrom": -1, "frames": [
                    { "row": 1, "column": 0, "duration": 160 },
                    { "row": 1, "column": 1, "duration": 220 },
                    { "row": 1, "column": 2, "duration": 220 },
                    { "row": 1, "column": 3, "duration": 280 }
                ] },
                "drag": { "loopFrom": 0, "frames": [
                    { "row": 2, "column": 0, "duration": 240 },
                    { "row": 2, "column": 1, "duration": 240 }
                ] },
                "land": { "loopFrom": -1, "frames": [
                    { "row": 2, "column": 2, "duration": 140 },
                    { "row": 2, "column": 3, "duration": 220 }
                ] },
                "sleep": { "loopFrom": 0, "frames": [
                    { "row": 3, "column": 0, "duration": 750 },
                    { "row": 3, "column": 1, "duration": 850 },
                    { "row": 3, "column": 2, "duration": 800 },
                    { "row": 3, "column": 3, "duration": 850 }
                ] },
                "practice": { "sheet": "practice", "loopFrom": 0, "frames": [
                    { "row": 0, "column": 0, "duration": 80 },
                    { "row": 0, "column": 1, "duration": 65 },
                    { "row": 0, "column": 2, "duration": 55 },
                    { "row": 0, "column": 3, "duration": 50 },
                    { "row": 0, "column": 4, "duration": 45 },
                    { "row": 0, "column": 5, "duration": 45 },
                    { "row": 1, "column": 0, "duration": 45 },
                    { "row": 1, "column": 1, "duration": 45 },
                    { "row": 1, "column": 2, "duration": 40 },
                    { "row": 1, "column": 3, "duration": 40 },
                    { "row": 1, "column": 4, "duration": 35 },
                    { "row": 1, "column": 5, "duration": 35 },
                    { "row": 2, "column": 0, "duration": 35 },
                    { "row": 2, "column": 1, "duration": 35 },
                    { "row": 2, "column": 2, "duration": 40 },
                    { "row": 2, "column": 3, "duration": 45 },
                    { "row": 2, "column": 4, "duration": 50 },
                    { "row": 2, "column": 5, "duration": 55 },
                    { "row": 3, "column": 0, "duration": 55 },
                    { "row": 3, "column": 1, "duration": 60 },
                    { "row": 3, "column": 2, "duration": 65 },
                    { "row": 3, "column": 3, "duration": 70 },
                    { "row": 3, "column": 4, "duration": 75 },
                    { "row": 3, "column": 5, "duration": 90 }
                ] },
                "rest": { "sheet": "practice", "loopFrom": -1, "frames": [
                    { "row": 0, "column": 0, "duration": 1000 }
                ] },
                "finish": { "loopFrom": -1, "frames": [
                    { "row": 5, "column": 0, "duration": 160 },
                    { "row": 5, "column": 1, "duration": 150 },
                    { "row": 5, "column": 2, "duration": 220 },
                    { "row": 5, "column": 3, "duration": 300 }
                ] },
                "look": { "sheet": "ambient", "loopFrom": -1, "frames": [
                    { "row": 0, "column": 0, "duration": 200 },
                    { "row": 0, "column": 1, "duration": 160 },
                    { "row": 0, "column": 2, "duration": 180 },
                    { "row": 0, "column": 3, "duration": 240 },
                    { "row": 1, "column": 0, "duration": 160 },
                    { "row": 1, "column": 1, "duration": 180 },
                    { "row": 1, "column": 2, "duration": 200 },
                    { "row": 1, "column": 3, "duration": 220 }
                ] },
                "stretch": { "sheet": "ambient", "loopFrom": -1, "frames": [
                    { "row": 2, "column": 0, "duration": 200 },
                    { "row": 2, "column": 1, "duration": 260 },
                    { "row": 2, "column": 2, "duration": 650 },
                    { "row": 2, "column": 3, "duration": 300 }
                ] },
                "yawn": { "sheet": "ambient", "loopFrom": -1, "frames": [
                    { "row": 3, "column": 0, "duration": 260 },
                    { "row": 3, "column": 1, "duration": 300 },
                    { "row": 3, "column": 2, "duration": 650 },
                    { "row": 3, "column": 3, "duration": 300 }
                ] },
                "wake": { "sheet": "ambient", "loopFrom": -1, "frames": [
                    { "row": 4, "column": 0, "duration": 120 },
                    { "row": 4, "column": 1, "duration": 120 },
                    { "row": 4, "column": 2, "duration": 140 },
                    { "row": 4, "column": 3, "duration": 140 },
                    { "row": 5, "column": 0, "duration": 160 },
                    { "row": 5, "column": 1, "duration": 220 },
                    { "row": 5, "column": 2, "duration": 140 },
                    { "row": 5, "column": 3, "duration": 160 }
                ] }
            } }
    ]
    readonly property var current: {
        for (let i = 0; i < root.available.length; ++i)
            if (root.available[i].id === root.requestedId) return root.available[i]
        return root.available[0]
    }
    readonly property bool fallback: root.current.id !== root.requestedId
}
