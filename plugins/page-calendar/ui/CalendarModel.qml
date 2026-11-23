import QtQuick
import "CalendarData.js" as Data

QtObject {
    id: root
    property bool active: true
    property bool previewMode: false
    property var previewPayload: null
    property bool initialized: false
    property bool followToday: true
    property string today: Data.gameDay()
    property string selectedDate: Data.gameDay()
    property string viewMonth: Data.monthStart(selectedDate)
    property string requestedWeek: ""
    property var days: ({})
    property var weeks: ({})
    readonly property bool loading: listQuery.busy || weekQuery.busy
    readonly property string minimumDate: Data.addDays(today, -15)
    readonly property string maximumDate: Data.addDays(today, 15)
    readonly property string monthLabel: qsTr("%1年%2月").arg(Number(viewMonth.slice(0, 4))).arg(Number(viewMonth.slice(5, 7)))
    readonly property bool canPreviousMonth: viewMonth > Data.monthStart(minimumDate)
    readonly property bool canNextMonth: viewMonth < Data.monthStart(maximumDate)
    readonly property var monthCells: Data.monthCells(viewMonth, today, selectedDate, minimumDate, maximumDate)
    readonly property string selectedLabel: qsTr("%1月%2日 · 周%3").arg(Number(selectedDate.slice(5, 7))).arg(Number(selectedDate.slice(8))).arg(Data.weekday(selectedDate))
    readonly property var entry: days[selectedDate] || null
    readonly property var weekly: weeks[Data.weekStart(selectedDate)] || null
    readonly property var dailyActivities: entry ? [
        { title: qsTr("大战"), value: entry.war },
        { title: qsTr("战场"), value: entry.battle },
        { title: qsTr("矿车"), value: entry.orecar },
        { title: qsTr("门派活动"), value: entry.school },
        { title: qsTr("驰援"), value: entry.rescue },
        { title: qsTr("秘境"), value: entry.card },
        { title: qsTr("福缘宠物"), value: entry.lucky },
        { title: qsTr("美人图"), value: entry.draw },
        { title: qsTr("世界首领"), value: entry.leader }
    ].filter(row => row.value.length > 0) : []
    readonly property var weeklyActivities: weekly ? [
        { title: qsTr("公共任务"), value: weekly.conn },
        { title: qsTr("团队秘境"), value: weekly.raid }
    ].filter(row => row.value.length > 0) : []

    readonly property CalendarRequest listQuery: CalendarRequest {
        onReceived: detail => root.acceptList(detail)
    }
    readonly property CalendarRequest weekQuery: CalendarRequest {
        onReceived: detail => root.acceptDay(detail)
    }
    readonly property Timer dayClock: Timer {
        interval: 60000
        repeat: true
        running: root.active && !root.previewMode
        onTriggered: { if (Data.gameDay() !== root.today) root.refresh() }
    }

    function selectDate(date) {
        if (!Data.validDate(date) || date < Data.addDays(today, -15) || date > Data.addDays(today, 15)) return
        followToday = date === today
        selectedDate = date
        viewMonth = Data.monthStart(date)
        requestWeek(date, false)
    }

    function moveMonth(amount) {
        const month = Data.addMonths(viewMonth, amount)
        if (month >= Data.monthStart(minimumDate) && month <= Data.monthStart(maximumDate)) viewMonth = month
    }

    function acceptList(detail) {
        if (!detail.today || !Data.validDate(detail.today.date) || !Array.isArray(detail.total)) return
        const anchor = detail.today.date
        const first = Data.addDays(anchor, -15)
        const last = Data.addDays(anchor, 15)
        const next = {}
        for (const raw of detail.total.slice(0, 64)) {
            const normalized = Data.normalize(raw)
            if (normalized && normalized.date >= first && normalized.date <= last) next[normalized.date] = normalized
        }
        if (!Object.keys(next).length) return
        const date = followToday || selectedDate < first || selectedDate > last ? anchor : selectedDate
        if (today !== anchor) weeks = ({})
        today = anchor
        days = next
        selectedDate = date
        if (viewMonth < Data.monthStart(first) || viewMonth > Data.monthStart(last)) viewMonth = Data.monthStart(date)
        requestWeek(date, false)
    }

    function acceptDay(detail) {
        const normalized = Data.normalize(detail)
        if (!normalized || normalized.date < Data.addDays(today, -15) || normalized.date > Data.addDays(today, 15)) return
        const nextDays = Object.assign({}, days)
        nextDays[normalized.date] = normalized
        days = nextDays
        if (detail.weekly && typeof detail.weekly === "object") {
            const nextWeeks = Object.assign({}, weeks)
            nextWeeks[Data.weekStart(normalized.date)] = normalized.weekly
            weeks = nextWeeks
        }
    }

    function requestWeek(date, force) {
        if (previewMode) return
        const offset = Data.offset(today, date)
        if (offset < 0 || offset > 15) return
        const key = Data.weekStart(date)
        if (!force && days[date] && weeks[key]) return
        if (!force && days[date] && weekQuery.busy && requestedWeek === key) return
        requestedWeek = key
        weekQuery.load("https://www.jx3api.com/active/calendar?mode=day&num=" + offset)
    }

    function applyPreview() {
        const anchor = Data.gameDay()
        const nextDays = {}
        const nextWeeks = {}
        if (previewPayload) {
            for (let offset = -15; offset <= 15; ++offset) {
                const date = Data.addDays(anchor, offset)
                const detail = Object.assign({}, previewPayload)
                detail.date = date
                const normalized = Data.normalize(detail, date)
                nextDays[date] = normalized
                nextWeeks[Data.weekStart(date)] = normalized.weekly
            }
        }
        today = anchor
        if (followToday) selectedDate = anchor
        days = nextDays
        weeks = nextWeeks
    }

    function refresh() {
        if (!initialized) return
        if (previewMode) { applyPreview(); return }
        listQuery.load("https://www.jx3api.com/active/calendar?mode=list&num=15")
        requestWeek(selectedDate, true)
    }

    onPreviewModeChanged: {
        if (initialized) { listQuery.cancel(); weekQuery.cancel(); refresh() }
    }
    onPreviewPayloadChanged: { if (initialized && previewMode) applyPreview() }
    Component.onCompleted: { initialized = true; refresh() }
}
