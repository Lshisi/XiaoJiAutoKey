#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>

#include <atomic>
#include <functional>
#include <mutex>
#include <thread>

// Asynchronous release checks, scheduled by UpdateCoordinator. Downloads and
// installation still require an explicit user action.
class UpdateService : public QObject {
    Q_OBJECT

public:
    explicit UpdateService(QObject* parent = nullptr);
    ~UpdateService() override;

    UpdateService(const UpdateService&) = delete;
    UpdateService& operator=(const UpdateService&) = delete;

    // Fetches the release list and reports the newest version. Runs on a
    // worker thread; results are delivered via checkFinished.
    void checkForUpdate();
    bool isBusy() const noexcept { return workerBusy_.load(); }

    // Downloads the release asset and unpacks it into a staging directory.
    // Results are delivered via downloadProgress / stageFinished.
    void downloadAndStageUpdate(const QString& assetUrl, const QString& version);

    // Replaces the installation next to the running exe with a previously
    // staged package. The caller must stop output first. Locked native DLLs
    // are deferred until restart; on failure the previous executable is restored.
    [[nodiscard]] static bool applyStagedUpdate(const QString& stagingDir, QString* errorMessage);

    // Removes leftovers from earlier updates (renamed old exe and staging
    // directory). Safe to call on every startup; performs no network access.
    static void cleanupPreviousUpdateArtifacts();

    // 启动时的同步清理可能因旧进程尚未退出、文件被杀软短暂占用而失败。
    // 在后台定时重试删除 .old.exe，直到成功或超时；不阻塞界面。
    static void retryOldExeCleanupInBackground(QObject* context);

    [[nodiscard]] static bool isVersionNewer(const QString& candidate, const QString& reference);
    [[nodiscard]] static QString normalizedVersion(const QString& version);
    [[nodiscard]] static QString releasePageUrl();

signals:
    void checkFinished(bool ok, const QString& message, bool hasUpdate,
        const QString& latestVersion, const QString& assetUrl, const QString& notes);
    void downloadProgress(qint64 received, qint64 total);
    void stageFinished(bool ok, const QString& message, const QString& stagingDir);

private:
    struct SessionGuard;

    bool fetchUrl(const QString& url, const std::function<bool(qint64, qint64)>& onProgress,
        QByteArray* body, unsigned long* statusCode, QString* error);
    void runWorker(std::function<void()> job);

    std::thread worker_;
    std::atomic<bool> workerBusy_{false};
    std::mutex sessionMutex_;
    void* session_ = nullptr;      // HINTERNET, kept as void* to avoid Windows.h here
    bool sessionClosed_ = false;   // set by the destructor to abort in-flight work
};
