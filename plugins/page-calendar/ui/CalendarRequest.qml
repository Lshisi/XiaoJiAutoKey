import QtQuick

QtObject {
    id: root
    property var pending: null
    property bool busy: false
    signal received(var detail)
    readonly property Timer deadline: Timer {
        interval: 15000
        onTriggered: root.cancel()
    }
    function cancel() {
        deadline.stop()
        const previous = pending
        pending = null
        busy = false
        if (previous) previous.abort()
    }
    function load(url) {
        cancel()
        const request = new XMLHttpRequest()
        pending = request
        busy = true
        request.onreadystatechange = function() {
            if (request.readyState !== XMLHttpRequest.DONE || root.pending !== request) return
            root.deadline.stop()
            root.pending = null
            root.busy = false
            if (request.status !== 200 || request.responseText.length > 2 * 1024 * 1024) return
            try {
                const response = JSON.parse(request.responseText)
                if (response && response.status === 200 && response.detail) root.received(response.detail)
            } catch (error) {
                return
            }
        }
        try {
            request.open("GET", url, true)
            deadline.restart()
            request.send()
        } catch (error) {
            cancel()
        }
    }
    Component.onDestruction: cancel()
}
