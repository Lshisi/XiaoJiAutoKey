#include "coordinators/UsageStatisticsCoordinator.h"

#include "AppTypes.h"
#include "ui/models/UsagePageModel.h"

#include <QTimeZone>
#include <algorithm>

UsageStatisticsCoordinator::UsageStatisticsCoordinator(QObject* parent) : QObject(parent) {}

void UsageStatisticsCoordinator::initialize(AppConfig& config, UsagePageModel& page) {
    config_ = &config;
    page_ = &page;
    clock_.start();
    lastWallTime_ = QDateTime::currentDateTime();
    refreshPage();
}

void UsageStatisticsCoordinator::recordElapsed() {
    if (!clock_.isValid()) return;
    const qint64 nowMs = clock_.elapsed();
    const QDateTime now = QDateTime::currentDateTime();
    qint64 remaining = nowMs - lastSampleMs_;
    if (active_ && remaining > 0) {
        QDateTime cursor = lastWallTime_;
        while (remaining > 0) {
            const QDateTime midnight = cursor.date().addDays(1).startOfDay(cursor.timeZone());
            const qint64 part = std::min(remaining, cursor.msecsTo(midnight));
            config_->dailyUsageMs[cursor.date().toString(Qt::ISODate).toStdString()] += part;
            cursor = cursor.addMSecs(part);
            remaining -= part;
        }
        dirty_ = true;
    }
    lastSampleMs_ = nowMs;
    lastWallTime_ = now;
}

void UsageStatisticsCoordinator::updateRunning(bool active) {
    if (!clock_.isValid()) return;
    recordElapsed();
    const bool changed = active_ != (active && !suspended_);
    const bool stopped = active_ && (!active || suspended_);
    active_ = active && !suspended_;
    const qint64 nowMs = clock_.elapsed();
    if (changed || nowMs - lastRefreshMs_ >= 1000) refreshPage();
    if (dirty_ && (stopped || nowMs - lastSaveAttemptMs_ >= 60000)) {
        lastSaveAttemptMs_ = nowMs;
        emit saveRequested();
    }
}

void UsageStatisticsCoordinator::refreshPage() {
    page_->setUsage(config_->dailyUsageMs, active_);
    lastRefreshMs_ = clock_.elapsed();
}

void UsageStatisticsCoordinator::markSaved() {
    dirty_ = false;
    if (clock_.isValid()) lastSaveAttemptMs_ = clock_.elapsed();
}

void UsageStatisticsCoordinator::setSuspended(bool suspended) {
    if (!clock_.isValid() || suspended_ == suspended) return;
    if (suspended) {
        recordElapsed();
        active_ = false;
        if (dirty_) emit saveRequested();
    } else {
        lastSampleMs_ = clock_.elapsed();
        lastWallTime_ = QDateTime::currentDateTime();
    }
    suspended_ = suspended;
    refreshPage();
}

void UsageStatisticsCoordinator::finish() {
    if (!clock_.isValid()) return;
    recordElapsed();
    active_ = false;
    if (dirty_) emit saveRequested();
    clock_.invalidate();
}
