.pragma library

var dayMilliseconds = 24 * 60 * 60 * 1000

function dateText(date) {
    return date.getUTCFullYear() + "-" + ("0" + (date.getUTCMonth() + 1)).slice(-2)
        + "-" + ("0" + date.getUTCDate()).slice(-2)
}

function gameDay() {
    return dateText(new Date(Date.now() + 60 * 60 * 1000))
}

function addDays(date, offset) {
    return dateText(new Date(Date.parse(date + "T00:00:00Z") + offset * dayMilliseconds))
}

function weekday(date) {
    return ["日", "一", "二", "三", "四", "五", "六"][new Date(date + "T00:00:00Z").getUTCDay()]
}

function validDate(value) {
    return typeof value === "string" && /^\d{4}-\d{2}-\d{2}$/.test(value)
        && dateText(new Date(value + "T00:00:00Z")) === value
}

function offset(from, to) {
    return Math.round((Date.parse(to + "T00:00:00Z") - Date.parse(from + "T00:00:00Z")) / dayMilliseconds)
}

function monthStart(date) { return date.slice(0, 7) + "-01" }

function addMonths(month, amount) {
    return dateText(new Date(Date.UTC(Number(month.slice(0, 4)), Number(month.slice(5, 7)) - 1 + amount, 1)))
}

function weekStart(date) {
    return addDays(date, -((new Date(date + "T00:00:00Z").getUTCDay() + 6) % 7))
}

function monthCells(month, today, selected, minimum, maximum) {
    var first = weekStart(month)
    var result = []
    for (var index = 0; index < 42; ++index) {
        var date = addDays(first, index)
        result.push({ date: date, day: Number(date.slice(8)), inMonth: date.slice(0, 7) === month.slice(0, 7),
            inRange: date >= minimum && date <= maximum, today: date === today, selected: date === selected })
    }
    return result
}

function text(value) {
    return typeof value === "string" ? value.trim().slice(0, 256) : ""
}

function list(value) {
    if (!Array.isArray(value)) return []
    return value.slice(0, 32).map(text).filter(function(item) { return item.length > 0 })
}

function normalize(detail, expectedDate) {
    if (!detail || typeof detail !== "object" || Array.isArray(detail) || !validDate(detail.date)
        || (expectedDate && detail.date !== expectedDate))
        return null
    var weekly = detail.weekly && typeof detail.weekly === "object" ? detail.weekly : {}
    return {
        date: detail.date,
        war: text(detail.war),
        battle: text(detail.battle),
        orecar: text(detail.orecar),
        school: text(detail.school),
        rescue: text(detail.rescue),
        card: list(detail.card).join("、"),
        lucky: list(detail.lucky).join("、"),
        draw: list(detail.draw).join("、"),
        leader: list(detail.leader).join("、"),
        weekly: { conn: list(weekly.conn).join("\n"), raid: list(weekly.raid).join("\n") }
    }
}
