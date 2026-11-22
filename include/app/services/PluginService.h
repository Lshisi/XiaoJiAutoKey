#pragma once

#include "AppTypes.h"
#include "core/plugins/NativePluginDriver.h"
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QObject>
#include <QPointer>
#include <QTimer>
#include <QStringList>
#include <QSet>
#include <functional>
#include <map>
#include <memory>
#include <thread>

class PluginCenterModel;
class PluginPageContext;
class QNetworkReply;
class QThread;
class QWidget;

namespace Plugins {
class PluginRuntime;
class PluginService final : public QObject {
    Q_OBJECT
public:
    struct ManagementResult { bool ok = false; bool restartRequired = false; QString message; };
    using DriverRemoval = std::function<ManagementResult()>;
    PluginService(AppConfig& config, QString hostDirectory, PluginCenterModel& model, QWidget* window);
    ~PluginService() override;
    void initialize();
    void shutdownRuntimes(const QString& reason = QStringLiteral("exit"));
    void reload(bool startup = false);
    void refreshCatalog(bool manual = false);
    void refreshBackgroundChecks();
    NativePluginDriver* openDriver(const QString& providerId, QString* error);
    NativePluginDriver* activeDriver() const;
    QString pluginDirectory(const QString& id) const;
    QString driverName(const QString& providerId) const;
    QVariantList drivers() const;
    QStringList deviceTokens() const;
    QStringList deviceTokensFor(const QString& pluginId) const;
    void setRuntimeState(const QVariantMap& state);
    void refreshTheme();
    void pollActiveDriver();
    void disconnectDrivers();
    void runManagement(const QString& providerId, const QString& operation);
    bool hasManagement(const QString& providerId, const QString& operation = {}) const;
    QVariantMap driverUi(const QString& providerId) const;
    void setDriverRemovalProvider(std::function<DriverRemoval(const QString&)> provider) { driverRemovalProvider_ = std::move(provider); }
    bool available(const QString& providerId, QString* error = nullptr) const;
    void confirmDriverActivation(const QString& providerId);
    bool rollbackDriverActivation(const QString& providerId);
    bool busy() const;
    void setHostBusy(bool busy);
    void notifyDeviceChange() { deviceChangePending_ = true; }
    void setConfigWriter(std::function<bool()> writer) { configWriter_ = std::move(writer); }

signals:
    void saveRequested();
    void navigationRequested(const QString& pageId);
    void driverSelectionRequested(const QString& providerId);
    void beforePluginDisabled(const QString& pluginId);
    void restartRequested();
    void engineRequested(bool macro, bool running);
    void inventoryChanged();
    void driverFailed(const QString& providerId, const QString& message);
    void dialogStateChanged(bool open);
    void managementStarting(const QString& providerId, const QString& operation);
    void managementFinished(const QString& providerId, const QString& operation, bool ok);

private:
    class DialogGuard;
    bool confirm(const QString& title, const QString& message, const QString& accept);
    struct Record {
        Manifest manifest;
        QString hash;
        QString origin;
        QString source;
        QString error;
        bool invalid = false;
        bool enabled = true;
        bool pending = false;
    };
    void dispatch(const QString& action, const QString& id, const QVariant& value);
    void updateModel();
    PluginPageContext* contextForPage(const QString& id);
    PluginPageContext* contextForPlugin(const QString& id);
    void syncRuntimes();
    void setBackgroundRunning(const QString& id, bool running);
    bool prepareActivation(const QString& id, const QString& operation, QString* error);
    void confirmActivation(const QString& id, const QString& operation);
    bool rollbackActivation(const QString& id, const QString& operation);
    void cancelActivations(const QString& id, bool runtimeOnly = false);
    void waitForHealthCheck();
    void download(const QString& url, qsizetype maximum, std::function<void(const QByteArray&)> completed,
        std::function<void(const QString&)> failed = {});
    void tryCatalogSources(QStringList sources, const QByteArray& key, const QString& primary, const QString& lastError = {});
    void downloadPackage(QStringList sources, const QString& id, const QString& hash, const QString& lastError = {});
    void stage(const QByteArray& zip, const QString& source, const QString& expectedId = {}, const QString& expectedHash = {});
    void installStaged(const QString& directory, const QString& hash, const Manifest& manifest, const QString& source);
    QString disableBlockedReason(const QString& id) const;
    bool bundledManifest(const QString& id, Manifest* manifest) const;
    void restoreBundled(const QString& id);
    void setEnabled(const QString& id, bool enabled, bool ask = true);
    void stopPlugin(const QString& id, const QString& reason);
    void executeManagement(const QString& providerId, const QString& operation, bool removeAfter = false);
    void completeRemoval(const QString& id);
    void remove(const QString& id);
    void loadCatalogCache();
    QString catalogCachePath() const;
    void fail(const QString& message);

    AppConfig& config_;
    QString hostDirectory_;
    QString cacheDirectory_;
    PluginCenterModel& model_;
    QWidget* window_ = nullptr;
    std::map<QString, Record> records_;
    std::map<QString, Manifest> bundledManifests_;
    std::map<QString, std::unique_ptr<NativePluginDriver>> drivers_;
    std::map<QString, PluginPageContext*> contexts_;
    std::map<QString, std::unique_ptr<PluginRuntime>> runtimes_;
    QSet<QString> suspendedRuntimes_;
    struct ActivationAttempt { QString version; QSet<QString> operations; };
    std::map<QString, ActivationAttempt> activationAttempts_;
    QJsonObject catalog_;
    QByteArray catalogEnvelope_;
    QNetworkAccessManager network_;
    QPointer<QNetworkReply> reply_;
    QPointer<QThread> worker_;
    QTimer refreshTimer_;
    QByteArray transfer_;
    QVariantMap runtimeState_;
    std::thread healthWorker_;
    std::atomic<bool> healthBusy_{false};
    qint64 lastHeartbeat_ = 0;
    quint64 healthGeneration_ = 0;
    bool shuttingDown_ = false;
    bool pageDialogOpen_ = false;
    int nativeDialogDepth_ = 0;
    QString healthFailure_;
    bool recoveryMode_ = false;
    bool hostBusy_ = false;
    bool deviceChangePending_ = false;
    std::function<bool()> configWriter_;
    std::function<DriverRemoval(const QString&)> driverRemovalProvider_;
};
}
