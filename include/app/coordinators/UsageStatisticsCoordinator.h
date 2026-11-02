#pragma once

#include <QDateTime>
#include <QElapsedTimer>
#include <QObject>

struct AppConfig;
class UsagePageModel;

class UsageStatisticsCoordinator final : public QObject {
    Q_OBJECT
public:
    explicit UsageStatisticsCoordinator(QObject* parent = nullptr);
    void initialize(AppConfig& config, UsagePageModel& page);
    void updateRunning(bool active);
    void recordElapsed();
    void markSaved();
    void setSuspended(bool suspended);
    void finish();

signals:
    void saveRequested();

private:
    void refreshPage();

    AppConfig* config_ = nullptr;
    UsagePageModel* page_ = nullptr;
    QElapsedTimer clock_;
    QDateTime lastWallTime_;
    qint64 lastSampleMs_ = 0;
    qint64 lastRefreshMs_ = 0;
    qint64 lastSaveAttemptMs_ = 0;
    bool active_ = false;
    bool suspended_ = false;
    bool dirty_ = false;
};
