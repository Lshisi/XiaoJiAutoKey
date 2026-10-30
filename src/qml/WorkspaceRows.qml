import QtQuick

ListModel {
    id: rows
    dynamicRoles: true
    property var sourceRows: []
    property string contextKey: ""
    property string loadedContextKey: ""
    property bool initialized: false
    property bool replacing: false
    signal contextReset()
    onSourceRowsChanged: Qt.callLater(rows.synchronize)
    onContextKeyChanged: Qt.callLater(rows.synchronize)
    Component.onCompleted: Qt.callLater(rows.synchronize)

    function keyFor(entry) {
        if (entry.id !== undefined)
            return "binding:" + entry.id
        // Macro steps have no IDs; equal steps share a presentation identity.
        return "step:" + JSON.stringify([entry.keyText, entry.delay, entry.positionHint])
    }

    function rowFor(entry) {
        return { rowKey: keyFor(entry), snapshot: JSON.stringify(entry), rowData: entry }
    }

    function synchronize() {
        const incoming = sourceRows || []
        if (!initialized || loadedContextKey !== contextKey) {
            replacing = true
            clear()
            for (let i = 0; i < incoming.length; ++i)
                append(rowFor(incoming[i]))
            loadedContextKey = contextKey
            initialized = true
            replacing = false
            contextReset()
            return
        }

        const remaining = Object.create(null)
        for (let i = 0; i < incoming.length; ++i) {
            const key = keyFor(incoming[i])
            remaining[key] = (remaining[key] || 0) + 1
        }
        for (let i = count - 1; i >= 0; --i) {
            const key = get(i).rowKey
            if (remaining[key])
                --remaining[key]
            else
                remove(i)
        }
        for (let i = 0; i < incoming.length; ++i) {
            const next = rowFor(incoming[i])
            let found = i
            while (found < count && get(found).rowKey !== next.rowKey)
                ++found
            if (found === count) {
                insert(i, next)
            } else {
                if (found !== i)
                    move(found, i, 1)
                if (get(i).snapshot !== next.snapshot) {
                    setProperty(i, "rowData", incoming[i])
                    setProperty(i, "snapshot", next.snapshot)
                }
            }
        }
    }
}
