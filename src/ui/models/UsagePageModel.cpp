#include "UsagePageModel.h"

#include <QDate>
#include <algorithm>
#include <array>

namespace {
constexpr int usageWindowDays = 270;

int usageLevel(qint64 milliseconds) {
    if (milliseconds <= 0) return 0;
    if (milliseconds < 15 * 60000) return 1;
    if (milliseconds < 60 * 60000) return 2;
    if (milliseconds < 180 * 60000) return 3;
    return 4;
}
}

UsagePageModel::UsagePageModel(QObject* parent)
    : PageModel(parent), year_(QDate::currentDate().year()) { refresh(); }

QString UsagePageModel::durationText(qint64 milliseconds) {
    const qint64 seconds = milliseconds / 1000;
    if (seconds >= 3600) return tr("%1小时%2分钟").arg(seconds / 3600).arg(seconds / 60 % 60);
    if (seconds >= 60) return tr("%1分钟").arg(seconds / 60);
    if (milliseconds > 0) return tr("%1秒").arg(std::max<qint64>(1, seconds));
    return tr("0分钟");
}

void UsagePageModel::setUsage(const std::map<std::string, std::int64_t>& dailyMs, bool running) {
    const QDate today = QDate::currentDate();
    if (year_ == state_.value("currentYear").toInt()) year_ = today.year();
    dailyMs_ = dailyMs;
    running_ = running;
    refresh();
}

void UsagePageModel::selectYear(int year) {
    if (year == year_ || !state_.value("years").toList().contains(QString::number(year))) return;
    year_ = year;
    refresh();
}

void UsagePageModel::refresh() {
    const QDate today = QDate::currentDate();
    const QDate firstDay = today.addDays(1 - usageWindowDays);
    qint64 totalMs = 0;
    qint64 todayMs = 0;
    qint64 periodMs = 0;
    int firstYear = today.year();
    int activeDays = 0;
    std::array<qint64, 12> monthlyMs{};
    for (const auto& [key, milliseconds] : dailyMs_) {
        const QDate date = QDate::fromString(QString::fromStdString(key), Qt::ISODate);
        if (!date.isValid() || milliseconds <= 0) continue;
        firstYear = std::min(firstYear, date.year());
        totalMs += milliseconds;
        if (date == today) todayMs = milliseconds;
        if (date.year() == year_) monthlyMs[date.month() - 1] += milliseconds;
        if (date >= firstDay && date <= today) {
            periodMs += milliseconds;
            ++activeDays;
        }
    }

    QVariantList years;
    for (int year = today.year(); year >= firstYear; --year) years.append(QString::number(year));
    const QDate gridStart = firstDay.addDays(1 - firstDay.dayOfWeek());
    const int weeks = static_cast<int>(gridStart.daysTo(today) / 7 + 1);
    QVariantList days;
    days.reserve(weeks * 7);
    for (int index = 0; index < weeks * 7; ++index) {
        const QDate date = gridStart.addDays(index);
        const QString key = date.toString(Qt::ISODate);
        const auto it = dailyMs_.find(key.toStdString());
        const qint64 milliseconds = it == dailyMs_.end() ? 0 : it->second;
        const bool inRange = date >= firstDay && date <= today;
        const QString tooltip = tr("%1\n%2").arg(date.toString(tr("yyyy年M月d日")), durationText(milliseconds));
        days.append(QVariantMap{{"date", key}, {"inRange", inRange},
            {"today", date == today}, {"level", usageLevel(milliseconds)}, {"tooltip", tooltip}});
    }
    QVariantList months;
    int previousMonth = 0;
    for (int column = 0; column < weeks; ++column) {
        const QDate date = std::min(today, gridStart.addDays(column * 7 + 6));
        if (date.month() == previousMonth) continue;
        months.append(QVariantMap{{"label", tr("%1月").arg(date.month())}, {"column", column}});
        previousMonth = date.month();
    }
    const qint64 maximumMonth = std::max<qint64>(60000, *std::max_element(monthlyMs.begin(), monthlyMs.end()));
    const qint64 axisUnit = maximumMonth >= 36000000 ? 36000000 : maximumMonth >= 3600000 ? 3600000
        : maximumMonth >= 600000 ? 600000 : 60000;
    const qint64 axisMaximum = (maximumMonth + 2 * axisUnit - 1) / (2 * axisUnit) * (2 * axisUnit);
    const auto axisText = [](qint64 milliseconds) {
        return milliseconds % 3600000 == 0 ? tr("%1小时").arg(milliseconds / 3600000)
            : tr("%1分").arg(milliseconds / 60000);
    };
    const QVariantList monthlyTicks{
        QVariantMap{{"ratio", 1.0}, {"label", axisText(axisMaximum)}},
        QVariantMap{{"ratio", 0.5}, {"label", axisText(axisMaximum / 2)}},
        QVariantMap{{"ratio", 0.0}, {"label", QStringLiteral("0")}}
    };
    QVariantList monthlyUsage;
    for (int month = 1; month <= 12; ++month) {
        const qint64 milliseconds = monthlyMs[month - 1];
        const QDate monthDate(year_, month, 1);
        const bool future = monthDate > today;
        const bool current = year_ == today.year() && month == today.month();
        const QString duration = milliseconds >= 3600000
            ? tr("%1小时%2分").arg(milliseconds / 3600000).arg(milliseconds / 60000 % 60)
            : durationText(milliseconds);
        const QString tooltip = future ? tr("%1年%2月\n尚未到来").arg(year_).arg(month)
            : tr("%1年%2月\n%3").arg(year_).arg(month).arg(durationText(milliseconds));
        monthlyUsage.append(QVariantMap{{"label", tr("%1月").arg(month)},
            {"ratio", static_cast<double>(milliseconds) / axisMaximum},
            {"duration", future ? tr("—") : duration}, {"current", current}, {"future", future},
            {"tooltip", tooltip}});
    }
    QVariantMap next{{"todayDuration", durationText(todayMs)}, {"totalDuration", durationText(totalMs)},
        {"periodDuration", durationText(periodMs)}, {"activeDays", activeDays}, {"hasUsage", periodMs > 0},
        {"running", running_}, {"year", year_}, {"currentYear", today.year()},
        {"rangeDays", usageWindowDays},
        {"periodLabel", tr("%1 — %2").arg(firstDay.toString("yyyy.MM.dd"), today.toString("yyyy.MM.dd"))},
        {"years", years}, {"yearIndex", today.year() - year_},
        {"weeks", weeks}, {"days", days}, {"months", months}, {"monthlyUsage", monthlyUsage},
        {"monthlyTicks", monthlyTicks}};
    if (next == state_) return;
    state_ = std::move(next);
    emit stateChanged();
}
