#pragma once

#include "PageModel.h"

#include <cstdint>
#include <map>
#include <string>

class UsagePageModel final : public PageModel {
    Q_OBJECT
public:
    explicit UsagePageModel(QObject* parent = nullptr);
    void setUsage(const std::map<std::string, std::int64_t>& dailyMs, bool running);
    Q_INVOKABLE void selectYear(int year);

private:
    static QString durationText(qint64 milliseconds);
    void refresh();

    std::map<std::string, std::int64_t> dailyMs_;
    int year_;
    bool running_ = false;
};
