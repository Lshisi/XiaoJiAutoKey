#include "services/PluginService.h"
#include "core/plugins/PluginArchive.h"
#include "core/plugins/PluginCatalog.h"
#include "core/plugins/PluginIntegrity.h"
#include "core/plugins/NativePluginModule.h"
#include "dialogs/AppMessageDialog.h"
#include "dialogs/ConfigConfirmDialog.h"
#include "ui/components/PluginPageContext.h"
#include "ui/components/PluginRuntime.h"
#include "ui/components/QmlIconProvider.h"
#include "services/PluginNativeBackend.h"
#include "ui/models/PluginCenterModel.h"
#include "DiagLog.h"

#include <QDateTime>
#include <QDesktopServices>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkProxyFactory>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSaveFile>
#include <QProcess>
#include <QThread>
#include <QUrl>
#include <QUuid>
#include <algorithm>
#include <exception>
#include <utility>
#include <Windows.h>

namespace Plugins {
namespace {
const QStringList kBuiltinPluginIds{QStringLiteral("xiaoji.driver.xiaoji"), QStringLiteral("xiaoji.driver.dd")};
QString preferenceId(const QString& provider) { return provider.section(QLatin1Char('/'), 0, 0); }
bool supportedBuiltin(const QString& id) {
    return kBuiltinPluginIds.contains(id);
}
QString driverActivation(const QString& provider) { return QStringLiteral("driver/") + provider.section(QLatin1Char('/'), 1); }
bool activationVerified(const PluginPreferences& preference, const QString& operation) {
    const auto name = operation.toStdString();
    return std::find(preference.verifiedActivations.cbegin(), preference.verifiedActivations.cend(), name) != preference.verifiedActivations.cend();
}
QSet<QString> activationSteps(const Manifest& manifest) {
    QSet<QString> result;
    if (manifest.native.isEmpty()) return result;
    for (const auto& driver : manifest.drivers) result.insert(QStringLiteral("driver/") + driver.id);
    if (manifest.backgroundNative) result.insert(QStringLiteral("background"));
    else for (const auto& command : manifest.commands) if (command.target == QStringLiteral("native")) result.insert(QStringLiteral("backend"));
    return result;
}
int driverOrder(const QVariant& value) {
    const auto id = preferenceId(value.toMap().value("id").toString());
    if (id == QStringLiteral("xiaoji.driver.xiaoji")) return 0;
    if (id == QStringLiteral("xiaoji.driver.dd")) return 1;
    if (id == QStringLiteral("xiaoji.driver.usb")) return 2;
    return 3;
}
QJsonObject readObject(const QString& path, qint64 maximum = 65536) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly) || file.size() > maximum) return {};
    return QJsonDocument::fromJson(file.readAll()).object();
}
bool writeBytes(const QString& path, const QByteArray& data) {
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) return false;
    QSaveFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(data) == data.size() && file.commit();
}
bool safeUrl(const QString& address) {
    const QUrl url(address);
    return url.isValid() && url.scheme() == QStringLiteral("https") && !url.host().isEmpty()
        && url.userInfo().isEmpty() && url.fragment().isEmpty() && (url.port(-1) == -1 || url.port() == 443);
}
}

class PluginService::DialogGuard {
public:
    explicit DialogGuard(PluginService& service) : service_(service) { ++service_.nativeDialogDepth_; emit service_.dialogStateChanged(true); }
    ~DialogGuard() { --service_.nativeDialogDepth_; emit service_.dialogStateChanged(service_.pageDialogOpen_ || service_.nativeDialogDepth_ > 0); }
private:
    PluginService& service_;
};

bool PluginService::confirm(const QString& title, const QString& message, const QString& accept) {
    const DialogGuard guard(*this);
    return ConfigConfirmDialog::confirm(window_, title, message, accept);
}

PluginService::PluginService(AppConfig& config, QString hostDirectory, PluginCenterModel& model, QWidget* window)
    : QObject(window), config_(config), hostDirectory_(std::move(hostDirectory)),
      cacheDirectory_(QDir(hostDirectory_).filePath(QStringLiteral("plugin-cache"))), model_(model), window_(window), network_(this) {
    QNetworkProxyFactory::setUseSystemConfiguration(true);
    network_.setTransferTimeout(30000);
    network_.setRedirectPolicy(QNetworkRequest::NoLessSafeRedirectPolicy);
    connect(&model_, &PluginCenterModel::actionRequested, this, &PluginService::dispatch);
    connect(&model_, &PluginCenterModel::stateChanged, this, &PluginService::refreshPendingCatalog, Qt::QueuedConnection);
    connect(this, &PluginService::dialogStateChanged, this, &PluginService::refreshPendingCatalog, Qt::QueuedConnection);
    model_.setPageContextProvider([this](const QString& id) { return contextForPage(id); });
    refreshTimer_.setInterval(6 * 60 * 60 * 1000);
    connect(&refreshTimer_, &QTimer::timeout, this, [this] { if (config_.automaticUpdateChecks) refreshCatalog(); });
    connect(qApp, &QCoreApplication::aboutToQuit, this, [this] { shuttingDown_ = true; });
}

PluginService::~PluginService() {
    shuttingDown_ = true;
    shutdownRuntimes();
    model_.setPageContextProvider({});
    if (reply_) { reply_->disconnect(this); reply_->abort(); }
    if (worker_) worker_->wait();
    if (healthWorker_.joinable()) healthWorker_.join();
    for (auto& [id, context] : contexts_) context->deactivate();
    disconnectDrivers();
    QStringList canceled;
    for (const auto& [id, attempt] : activationAttempts_) canceled.append(id);
    for (const auto& id : canceled) cancelActivations(id);
}

void PluginService::shutdownRuntimes(const QString& reason) {
    shuttingDown_ = true;
    QStringList canceled;
    for (auto& [id, runtime] : runtimes_) {
        canceled.append(id);
        disconnect(runtime.get(), &PluginRuntime::changed, this, &PluginService::updateModel);
        runtime->shutdown(reason);
    }
    runtimes_.clear();
    for (const auto& id : canceled) cancelActivations(id, true);
    for (auto& [id, context] : contexts_) context->deactivate();
}

void PluginService::initialize() {
    recoveryMode_ = QCoreApplication::arguments().contains(QStringLiteral("--disable-third-party-plugins"));
    for (auto& [id, preference] : config_.plugins) {
        if (recoveryMode_ && !supportedBuiltin(QString::fromStdString(id))) preference.enabled = false;
        if (!preference.activationInProgress) continue;
        preference.activeVersion = preference.rollbackVersion;
        if (preference.activeVersion.empty() && !supportedBuiltin(QString::fromStdString(id))) preference.enabled = false;
        preference.activationInProgress = false;
        preference.trialVersion.clear();
        preference.verifiedActivations.clear();
        model_.setStatus(tr("上次插件激活未完成，已恢复旧版本或停用异常插件。"));
    }
    if (recoveryMode_) {
        auto& defaultBuiltin = config_.plugins["xiaoji.driver.xiaoji"];
        defaultBuiltin.enabled = true;
        defaultBuiltin.removed = false;
        defaultBuiltin.removalPendingRestart = false;
        defaultBuiltin.removalError.clear();
        for (const auto& id : kBuiltinPluginIds) {
            auto& preference = config_.plugins[id.toStdString()];
            preference.pendingVersion.clear(); preference.trialVersion.clear(); preference.verifiedActivations.clear();
            preference.activationInProgress = false;
        }
        config_.activeDriverId = "xiaoji.driver.xiaoji/default";
        config_.driverMode = DriverMode::Xiaoji;
    }
    loadCatalogCache();
    reload(true);
    refreshTimer_.start();
    refreshBackgroundChecks();
}

void PluginService::loadCatalogCache() {
    catalog_ = {};
    catalogEnvelope_.clear();
    const auto key = QByteArray::fromBase64(QByteArray::fromStdString(config_.pluginRegistryPublicKey));
    QFile file(catalogCachePath());
    if (key.size() != 65 || !file.open(QIODevice::ReadOnly) || file.size() > 8 * 1024 * 1024) return;
    const auto bytes = file.readAll();
    QString error;
    if (decodeCatalog(bytes, key, 0, &catalog_, &error)) catalogEnvelope_ = bytes;
    else DiagLog::Write("plugin catalog cache: " + error.toStdString());
}

QString PluginService::catalogCachePath() const {
    const auto source = QByteArray::fromStdString(config_.pluginRegistryUrl) + '\n' + QByteArray::fromStdString(config_.pluginRegistryPublicKey);
    const auto identity = QString::fromLatin1(QCryptographicHash::hash(source, QCryptographicHash::Sha256).toHex());
    return QDir(cacheDirectory_).filePath(QStringLiteral("catalog-") + identity + QStringLiteral(".json"));
}

bool PluginService::busy() const { return reply_ || worker_ || nativeDialogDepth_ > 0 || hostBusy_ || catalogRefreshing_; }

void PluginService::setHostBusy(bool busy) {
    if (hostBusy_ == busy) return;
    hostBusy_ = busy;
    model_.setBusy(hostBusy_ || reply_ || worker_);
}

void PluginService::reload(bool startup) {
    QSet<QString> previouslyAvailable;
    for (const auto& [id, record] : records_)
        if (record.enabled && record.error.isEmpty()) previouslyAvailable.insert(id);
    bundledManifests_.clear();
    std::map<QString, QVector<Record>> candidates;
    for (const auto& kind : {QStringLiteral("builtin"), QStringLiteral("installed")}) {
        const QDir base(QDir(hostDirectory_).filePath(QStringLiteral("plugins/") + kind));
        for (const auto& pluginFolder : base.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks)) {
            const QString id = pluginFolder.fileName();
            if (!validPluginId(id) || (kind == QStringLiteral("builtin") && !supportedBuiltin(id))) continue;
            if (recoveryMode_ && kind == QStringLiteral("installed") && supportedBuiltin(id)) continue;
            auto& preference = config_.plugins[id.toStdString()];
            if (preference.removalPendingRestart) preference.enabled = false;
            if (recoveryMode_ && !supportedBuiltin(id)) preference.enabled = false;
            if (preference.removed && kind != QStringLiteral("builtin")) {
                if (startup && kind == QStringLiteral("installed")) {
                    const auto path = pluginFolder.canonicalFilePath();
                    const auto allowed = base.canonicalPath() + QLatin1Char('/');
                    if (path.startsWith(allowed, Qt::CaseInsensitive)) QDir(path).removeRecursively();
                }
                continue;
            }
            const QDir directory(pluginFolder.absoluteFilePath());
            int count = 0;
            for (const auto& versionFolder : directory.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks)) {
                if (++count > 128 || !validVersion(versionFolder.fileName())) continue;
                Record record;
                QString error;
                const auto manifestPath = QDir(versionFolder.absoluteFilePath()).filePath(QStringLiteral("plugin.json"));
                if (!readManifest(manifestPath, &record.manifest, &error)
                    || record.manifest.id != id || record.manifest.version != versionFolder.fileName()) {
                    if (error.isEmpty()) error = tr("插件清单的 ID 或版本与安装目录不一致。");
                    DiagLog::Write("plugin[" + id.toStdString() + "] " + error.toStdString());
                    const auto metadata = readObject(manifestPath, 128 * 1024);
                    record.manifest = {};
                    record.manifest.id = id;
                    record.manifest.version = versionFolder.fileName();
                    record.manifest.directory = versionFolder.canonicalFilePath();
                    record.manifest.name = metadata.value(QStringLiteral("name")).toString(id).left(80);
                    if (record.manifest.name.trimmed().isEmpty()) record.manifest.name = id;
                    record.manifest.author = metadata.value(QStringLiteral("author")).toString().left(120);
                    record.manifest.description = metadata.value(QStringLiteral("description")).toString(tr("插件无法加载，请查看失败原因或重新安装。")).left(2000);
                    const auto minimum = metadata.value(QStringLiteral("minHostVersion")).toString();
                    const auto maximum = metadata.value(QStringLiteral("maxHostVersion")).toString();
                    if (validVersion(minimum)) record.manifest.minHostVersion = minimum;
                    if (validVersion(maximum)) record.manifest.maxHostVersion = maximum;
                    record.invalid = true;
                    record.error = error;
                }
                const auto info = readObject(QDir(record.manifest.directory).filePath(QStringLiteral(".installation.json")));
                record.hash = info.value(QStringLiteral("sha256")).toString();
                record.origin = info.value(QStringLiteral("origin")).toString() == QStringLiteral("catalog")
                    ? QStringLiteral("catalog") : QStringLiteral("local");
                record.source = info.value(QStringLiteral("source")).toString(record.manifest.directory);
                if (record.origin == QStringLiteral("catalog") && QUrl(record.source).scheme() != QStringLiteral("https"))
                    record.origin = QStringLiteral("local");
                if (kind == QStringLiteral("builtin")) record.origin = QStringLiteral("bundled");
                if (kind == QStringLiteral("installed") && record.invalid) record.integrityError = true;
                record.manifest.builtin = supportedBuiltin(id);
                record.enabled = preference.enabled && !preference.removed;
                if (record.error.isEmpty() && kind == QStringLiteral("installed")) {
                    const auto plugin = catalogPlugin(catalog_, id);
                    const auto exact = catalogVersion(catalog_, id, record.manifest.version);
                    const bool matchesCatalog = validSha256(record.hash)
                        && exact.value(QStringLiteral("sha256")).toString().compare(record.hash, Qt::CaseInsensitive) == 0;
                    if (matchesCatalog) {
                        record.manifest.contentsSha256 = exact.value(QStringLiteral("contentsSha256")).toString();
                        if (!validSha256(record.manifest.contentsSha256))
                            record.error = tr("此版本缺少可信文件校验信息，请刷新插件目录后重试。");
                        else if ((!info.value(QStringLiteral("publisher")).toString().isEmpty()
                                && info.value(QStringLiteral("publisher")).toString() != plugin.value(QStringLiteral("publisher")).toString())
                            || (!info.value(QStringLiteral("repository")).toString().isEmpty()
                                && info.value(QStringLiteral("repository")).toString().compare(plugin.value(QStringLiteral("repository")).toString(), Qt::CaseInsensitive) != 0))
                            record.error = tr("插件发布来源已变化，请重新安装并确认新来源。");
                        else record.contentVerified = verifyManifestContents(record.manifest, &record.error);
                    } else if (record.origin == QStringLiteral("catalog") || id.startsWith(QStringLiteral("xiaoji."))) {
                        record.error = tr("无法验证此版本的发布来源，请刷新目录或重新安装。");
                    } else {
                        record.manifest.contentsSha256 = info.value(QStringLiteral("contentsSha256")).toString();
                        (void)verifyManifestContents(record.manifest, &record.error);
                    }
                    if (!record.error.isEmpty()) { record.integrityError = true; record.invalid = true; }
                }
                if (record.error.isEmpty()) record.error = blockedReason(catalog_, id, record.manifest.version, record.hash);
                if (record.error.isEmpty() && kind == QStringLiteral("installed") && id.startsWith(QStringLiteral("xiaoji."))) {
                    const auto metadata = catalogVersion(catalog_, id, record.manifest.version);
                    if (!catalogPlugin(catalog_, id).value(QStringLiteral("official")).toBool()
                        || record.hash.isEmpty() || metadata.value(QStringLiteral("sha256")).toString().compare(record.hash, Qt::CaseInsensitive) != 0)
                        record.error = tr("此包不能验证为官方插件，保留停用状态。");
                }
                if (kind == QStringLiteral("builtin") && record.error.isEmpty()
                    && (!bundledManifests_.contains(id) || compareVersions(record.manifest.version, bundledManifests_.at(id).version) > 0))
                    bundledManifests_[id] = record.manifest;
                candidates[id].push_back(record);
            }
            if (!candidates.contains(id)) {
                Record record;
                record.manifest.id = id;
                record.manifest.name = id;
                record.manifest.version = QString::fromStdString(preference.activeVersion);
                record.manifest.directory = pluginFolder.canonicalFilePath();
                record.manifest.builtin = supportedBuiltin(id);
                record.origin = kind == QStringLiteral("builtin") ? QStringLiteral("bundled") : QStringLiteral("local");
                record.source = record.manifest.directory;
                record.enabled = preference.enabled && !preference.removed;
                record.invalid = true;
                record.error = tr("插件目录中没有有效的版本文件夹，请重新安装或卸载此插件。");
                candidates[id].push_back(record);
            }
        }
    }
    for (const auto& [name, preference] : config_.plugins) {
        const auto id = QString::fromStdString(name);
        if (!validPluginId(id) || preference.removed || candidates.contains(id)
            || (preference.activeVersion.empty() && preference.pendingVersion.empty())) continue;
        Record record;
        record.manifest.id = id;
        record.manifest.name = id;
        record.manifest.version = QString::fromStdString(preference.activeVersion.empty() ? preference.pendingVersion : preference.activeVersion);
        record.manifest.builtin = supportedBuiltin(id);
        record.enabled = preference.enabled;
        record.invalid = true;
        record.error = tr("已安装插件的文件缺失，请重新安装或卸载此插件。");
        candidates[id].push_back(record);
    }
    records_.clear();
    for (auto& [id, versions] : candidates) {
        auto& preference = config_.plugins[id.toStdString()];
        if (startup && !preference.pendingVersion.empty()) {
            const auto pending = QString::fromStdString(preference.pendingVersion);
            const auto found = std::find_if(versions.cbegin(), versions.cend(), [&](const Record& item) {
                return item.manifest.version == pending && item.error.isEmpty();
            });
            if (found != versions.cend()) {
                preference.rollbackVersion = preference.activeVersion;
                preference.activeVersion = preference.pendingVersion;
                preference.trialVersion = preference.pendingVersion;
                preference.verifiedActivations.clear();
                preference.activationInProgress = false;
            }
            preference.pendingVersion.clear();
        }
        std::stable_sort(versions.begin(), versions.end(), [](const Record& left, const Record& right) {
            if (left.invalid != right.invalid) return !left.invalid;
            return compareVersions(left.manifest.version, right.manifest.version) > 0;
        });
        auto chosen = versions.begin();
        if (!preference.activeVersion.empty()) {
            const auto requested = QString::fromStdString(preference.activeVersion);
            const auto pinned = std::find_if(versions.begin(), versions.end(), [&](const Record& item) { return item.manifest.version == requested; });
            if (pinned != versions.end()) chosen = pinned;
            else if (!recoveryMode_) {
                chosen->enabled = supportedBuiltin(id);
                chosen->error = tr("已选择的插件版本缺失，请重新安装该版本或准备更新后重启。");
                preference.enabled = supportedBuiltin(id);
            }
        }
        if (startup && !preference.removed && !chosen->invalid && (preference.activeVersion.empty() || (recoveryMode_ && chosen->origin == QStringLiteral("bundled"))))
            preference.activeVersion = chosen->manifest.version.toStdString();
        if (preference.trialVersion == chosen->manifest.version.toStdString() && activationSteps(chosen->manifest).isEmpty()) {
            preference.trialVersion.clear(); preference.verifiedActivations.clear(); preference.activationInProgress = false;
        }
        chosen->pending = !preference.pendingVersion.empty();
        records_.emplace(id, *chosen);
    }
    if (startup && !hasAvailableBuiltin()) {
        for (const auto& id : kBuiltinPluginIds) {
            const auto found = records_.find(id);
            if (found == records_.end() || found->second.invalid || !found->second.error.isEmpty()
                || found->second.manifest.drivers.isEmpty()) continue;
            auto& preference = config_.plugins[id.toStdString()];
            if (preference.removed || preference.removalPendingRestart || (found->second.pending && preference.activeVersion.empty())) continue;
            preference.enabled = true;
            preference.removed = false;
            if (preference.activeVersion.empty()) preference.activeVersion = found->second.manifest.version.toStdString();
            found->second.enabled = true;
            if (hasAvailableBuiltin()) break;
        }
    }
    for (const auto& id : previouslyAvailable) {
        const auto current = records_.find(id);
        if (current != records_.end() && current->second.enabled && current->second.error.isEmpty()) continue;
        emit beforePluginDisabled(id);
        waitForHealthCheck();
        for (auto& [provider, driver] : drivers_) if (provider.startsWith(id + QLatin1Char('/'))) {
            driver->ReleaseAll(); driver->setEnabled(false); driver->disconnect();
        }
        cancelActivations(id);
    }
    bool selectionRecovered = false;
    if (config_.activeDriverId.empty() || !available(QString::fromStdString(config_.activeDriverId))) {
        const auto providers = drivers();
        if (!config_.activeDriverId.empty()) { config_.activeDriverId.clear(); selectionRecovered = true; }
        if (!providers.isEmpty()) {
            config_.activeDriverId = providers.front().toMap().value("id").toString().toStdString();
            if (config_.activeDriverId == "xiaoji.driver.xiaoji/default") config_.driverMode = DriverMode::Xiaoji;
            else if (config_.activeDriverId == "xiaoji.driver.dd/default") config_.driverMode = DriverMode::Legacy;
            else if (config_.activeDriverId == "xiaoji.driver.usb/default") config_.driverMode = DriverMode::Usb;
            selectionRecovered = true;
        }
    }
    for (auto iterator = runtimes_.begin(); iterator != runtimes_.end();) {
        const auto found = records_.find(iterator->first);
        if (found == records_.end() || !found->second.enabled || !found->second.error.isEmpty()
            || iterator->second->version() != found->second.manifest.version || iterator->second->directory() != found->second.manifest.directory) {
            disconnect(iterator->second.get(), &PluginRuntime::changed, this, &PluginService::updateModel);
            iterator->second->shutdown(found == records_.end() ? QStringLiteral("removed") : !found->second.error.isEmpty()
                ? QStringLiteral("blocked") : !found->second.enabled ? QStringLiteral("disabled") : QStringLiteral("updated"));
            const auto id = iterator->first;
            iterator = runtimes_.erase(iterator);
            cancelActivations(id, true);
        } else ++iterator;
    }
    for (auto iterator = contexts_.begin(); iterator != contexts_.end();) {
        const auto found = records_.find(iterator->first);
        if (found == records_.end() || !found->second.enabled || !found->second.error.isEmpty()
            || iterator->second->property("pluginVersion").toString() != found->second.manifest.version
            || iterator->second->property("pluginDirectory").toString() != found->second.manifest.directory) {
            iterator->second->deactivate(); iterator->second->deleteLater(); iterator = contexts_.erase(iterator);
        } else ++iterator;
    }
    syncRuntimes();
    updateModel();
    emit inventoryChanged();
    if (startup || selectionRecovered) emit saveRequested();
}

QVariantList PluginService::drivers() const {
    QVariantList result;
    for (const auto& [id, record] : records_) {
        if (!record.enabled || !record.error.isEmpty() || suspendedRuntimes_.contains(id)) continue;
        const auto preference = config_.plugins.find(id.toStdString());
        if (record.pending && preference != config_.plugins.end() && preference->second.activeVersion.empty()) continue;
        for (const auto& provider : record.manifest.drivers)
            result.append(QVariantMap{{"id", record.manifest.providerId(provider.id)}, {"name", provider.name},
                {"pluginId", id}, {"pending", record.pending}});
    }
    std::stable_sort(result.begin(), result.end(), [](const QVariant& left, const QVariant& right) { return driverOrder(left) < driverOrder(right); });
    return result;
}

QStringList PluginService::deviceTokens() const {
    QStringList tokens;
    for (const auto& [id, record] : records_)
        for (const auto& driver : record.manifest.drivers) tokens.append(driver.deviceTokens);
    tokens.removeDuplicates();
    return tokens;
}

QStringList PluginService::deviceTokensFor(const QString& pluginId) const {
    QStringList result;
    const auto found = records_.find(pluginId);
    if (found != records_.end()) for (const auto& driver : found->second.manifest.drivers) result.append(driver.deviceTokens);
    return result;
}

QString PluginService::pluginDirectory(const QString& id) const {
    const auto found = records_.find(id);
    return found == records_.end() ? QString() : found->second.manifest.directory;
}

QString PluginService::driverName(const QString& providerId) const {
    for (const auto& entry : drivers()) if (entry.toMap().value("id").toString() == providerId) return entry.toMap().value("name").toString();
    return {};
}

bool PluginService::available(const QString& providerId, QString* error) const {
    const auto fail = [&](const QString& message) { if (error) *error = message; return false; };
    const auto found = records_.find(preferenceId(providerId));
    if (found == records_.end()) return fail(tr("所选驱动插件未安装或清单无效。"));
    const auto& record = found->second;
    if (!record.enabled) return fail(tr("所选驱动插件已停用。"));
    if (suspendedRuntimes_.contains(record.manifest.id)) return fail(tr("所选驱动插件正在执行管理操作。"));
    if (!record.error.isEmpty()) return fail(record.error);
    const auto preference = config_.plugins.find(record.manifest.id.toStdString());
    if (record.pending && preference != config_.plugins.end() && preference->second.activeVersion.empty()) return fail(tr("新驱动插件将在重启后可用。"));
    for (const auto& driver : record.manifest.drivers)
        if (record.manifest.providerId(driver.id) == providerId) return true;
    return fail(tr("插件没有提供所选驱动。"));
}

NativePluginDriver* PluginService::openDriver(const QString& providerId, QString* error) {
    if (!available(providerId, error)) return nullptr;
    waitForHealthCheck();
    healthFailure_.clear();
    const auto& manifest = records_.at(preferenceId(providerId)).manifest;
    if (!prepareActivation(manifest.id, driverActivation(providerId), error)) return nullptr;
    const auto cacheId = providerId + QLatin1Char('@') + manifest.version;
    const auto existing = drivers_.find(cacheId);
    if (existing != drivers_.end()) { existing->second->setEnabled(true); return existing->second.get(); }
    auto instance = NativePluginDriver::create(manifest, providerId.section(QLatin1Char('/'), 1), hostDirectory_,
        QDir(hostDirectory_).filePath(QStringLiteral("plugin-data/") + manifest.id), error);
    if (!instance) return nullptr;
    auto* result = instance.get();
    drivers_.emplace(cacheId, std::move(instance));
    return result;
}

bool PluginService::prepareActivation(const QString& id, const QString& operation, QString* error) {
    const auto record = records_.find(id);
    if (shuttingDown_ || record == records_.end() || !record->second.enabled || !record->second.error.isEmpty()) return false;
    if (!NativeModule::isLoaded(id) && !verifyRecordContents(id, error)) return false;
    auto& preference = config_.plugins[id.toStdString()];
    if (preference.trialVersion != record->second.manifest.version.toStdString() || activationVerified(preference, operation)) return true;
    auto& attempt = activationAttempts_[id];
    if (attempt.version != record->second.manifest.version) attempt = {record->second.manifest.version, {}};
    const bool alreadyPending = attempt.operations.contains(operation);
    attempt.operations.insert(operation);
    preference.activationInProgress = true;
    if (configWriter_ && configWriter_()) return true;
    if (!alreadyPending) attempt.operations.remove(operation);
    preference.activationInProgress = !attempt.operations.isEmpty();
    if (attempt.operations.isEmpty()) activationAttempts_.erase(id);
    if (error) *error = tr("无法保存插件激活恢复点，本次未加载新版本。");
    return false;
}

NativePluginDriver* PluginService::activeDriver() const {
    const auto id = QString::fromStdString(config_.activeDriverId);
    const auto record = records_.find(preferenceId(id));
    if (record == records_.end()) return nullptr;
    const auto found = drivers_.find(id + QLatin1Char('@') + record->second.manifest.version);
    return found == drivers_.end() ? nullptr : found->second.get();
}

void PluginService::confirmDriverActivation(const QString& providerId) {
    confirmActivation(preferenceId(providerId), driverActivation(providerId));
}

void PluginService::confirmActivation(const QString& id, const QString& operation) {
    const auto record = records_.find(id);
    if (record == records_.end()) return;
    auto& preference = config_.plugins[id.toStdString()];
    if (preference.trialVersion != record->second.manifest.version.toStdString() || activationVerified(preference, operation)) return;
    const auto steps = activationSteps(record->second.manifest);
    if (!steps.contains(operation)) return;
    preference.verifiedActivations.push_back(operation.toStdString());
    const auto attempt = activationAttempts_.find(id);
    if (attempt != activationAttempts_.end() && attempt->second.version == record->second.manifest.version) {
        attempt->second.operations.remove(operation);
        if (attempt->second.operations.isEmpty()) activationAttempts_.erase(attempt);
    }
    preference.activationInProgress = activationAttempts_.contains(id)
        && activationAttempts_.at(id).version == record->second.manifest.version && !activationAttempts_.at(id).operations.isEmpty();
    if (std::all_of(steps.cbegin(), steps.cend(), [&](const QString& step) { return activationVerified(preference, step); })) {
        preference.trialVersion.clear(); preference.verifiedActivations.clear();
    }
    emit saveRequested();
}

bool PluginService::rollbackDriverActivation(const QString& providerId) {
    return rollbackActivation(preferenceId(providerId), driverActivation(providerId));
}

void PluginService::cancelActivations(const QString& id, bool runtimeOnly) {
    const auto attempt = activationAttempts_.find(id);
    if (attempt == activationAttempts_.end()) return;
    const auto version = attempt->second.version;
    if (runtimeOnly) { attempt->second.operations.remove(QStringLiteral("background")); attempt->second.operations.remove(QStringLiteral("backend")); }
    else attempt->second.operations.clear();
    const bool pending = !attempt->second.operations.isEmpty();
    if (!pending) activationAttempts_.erase(attempt);
    auto& preference = config_.plugins[id.toStdString()];
    if (preference.trialVersion == version.toStdString()) {
        preference.activationInProgress = pending;
        emit saveRequested();
    }
}

bool PluginService::rollbackActivation(const QString& id, const QString& operation) {
    const auto record = records_.find(id);
    if (record == records_.end()) return false;
    auto& preference = config_.plugins[id.toStdString()];
    if (preference.trialVersion != record->second.manifest.version.toStdString() || activationVerified(preference, operation)
        || !activationSteps(record->second.manifest).contains(operation)) return false;
    emit beforePluginDisabled(id);
    waitForHealthCheck();
    for (auto& [provider, driver] : drivers_) if (provider.startsWith(id + QLatin1Char('/'))) {
        driver->ReleaseAll(); driver->setEnabled(false); driver->disconnect();
    }
    const auto runtime = runtimes_.find(id);
    if (runtime != runtimes_.end()) {
        disconnect(runtime->second.get(), &PluginRuntime::changed, this, nullptr);
        runtime->second->shutdown(QStringLiteral("rollback")); runtimes_.erase(runtime);
    }
    activationAttempts_.erase(id);
    const bool hasPrevious = !preference.rollbackVersion.empty() && preference.rollbackVersion != preference.activeVersion;
    preference.activeVersion = preference.rollbackVersion;
    preference.activationInProgress = false;
    preference.trialVersion.clear();
    preference.verifiedActivations.clear();
    if (!hasPrevious && !supportedBuiltin(id)) preference.enabled = false;
    emit saveRequested();
    reload();
    model_.setStatus(hasPrevious ? tr("插件激活失败，已恢复之前的版本。") : supportedBuiltin(id)
        ? tr("小叽插件激活失败，已保留内置插件，请恢复随包版本。") : tr("插件激活失败，已停用该插件。"));
    return hasPrevious;
}

void PluginService::disconnectDrivers() {
    waitForHealthCheck();
    for (auto& [id, driver] : drivers_) { driver->ReleaseAll(); driver->disconnect(); }
}

void PluginService::waitForHealthCheck() {
    ++healthGeneration_;
    if (healthWorker_.joinable()) healthWorker_.join();
    healthBusy_.store(false);
}

bool PluginService::hasManagement(const QString& providerId, const QString& operation) const {
    const auto found = records_.find(preferenceId(providerId));
    if (found == records_.end() || !found->second.error.isEmpty()) return false;
    const auto& commands = found->second.manifest.management;
    if ((operation.isEmpty() || operation == QStringLiteral("uninstall")) && driverRemovalProvider_ && driverRemovalProvider_(providerId)) return true;
    return operation.isEmpty() ? !commands.isEmpty() : commands.contains(operation)
        || (operation == QStringLiteral("repair") && commands.contains(QStringLiteral("install")));
}

QVariantMap PluginService::driverUi(const QString& providerId) const {
    const auto found = records_.find(preferenceId(providerId));
    if (found == records_.end() || !available(providerId)) return {};
    for (const auto& provider : found->second.manifest.drivers) {
        if (found->second.manifest.providerId(provider.id) != providerId) continue;
        return {{"diagnostics", provider.diagnostics}, {"selfCheck", provider.selfCheck},
            {"repair", hasManagement(providerId, QStringLiteral("repair"))},
            {"uninstall", hasManagement(providerId, QStringLiteral("uninstall"))}};
    }
    return {};
}

void PluginService::runManagement(const QString& providerId, const QString& requested) {
    if (busy()) { fail(tr("已有插件操作正在进行。")); return; }
    const auto found = records_.find(preferenceId(providerId));
    if (found == records_.end() || !found->second.enabled || !found->second.error.isEmpty()) return;
    const auto& manifest = found->second.manifest;
    QString operation = requested;
    if (operation == QStringLiteral("repair") && !manifest.management.contains(operation)) operation = QStringLiteral("install");
    if (!hasManagement(providerId, operation)) { fail(tr("插件没有提供此管理操作。")); return; }
    const auto description = manifest.management.contains(operation) ? manifest.management.value(operation).confirmation
        : tr("卸载此插件使用的系统驱动，保留插件本身。需要再次使用时，可主动安装或修复。可能需要重启电脑。");
    if (!confirm(tr("执行插件管理操作"), tr("%1\n\n%2").arg(manifest.name, description), tr("继续"))) return;
    executeManagement(providerId, operation);
}

void PluginService::stopPlugin(const QString& id, const QString& reason) {
    emit beforePluginDisabled(id);
    const auto runtime = runtimes_.find(id);
    if (runtime != runtimes_.end()) {
        disconnect(runtime->second.get(), &PluginRuntime::changed, this, nullptr);
        runtime->second->shutdown(reason); runtimes_.erase(runtime);
    }
    cancelActivations(id);
    const auto context = contexts_.find(id);
    if (context != contexts_.end()) { context->second->deactivate(); context->second->deleteLater(); contexts_.erase(context); }
    waitForHealthCheck();
    for (auto& [provider, driver] : drivers_) if (provider.startsWith(id + QLatin1Char('/'))) {
        driver->ReleaseAll(); driver->setEnabled(false); driver->disconnect();
    }
}

void PluginService::executeManagement(const QString& providerId, const QString& operation, bool removeAfter) {
    const DialogGuard guard(*this);
    const auto id = preferenceId(providerId);
    if (removeAfter) {
        const auto reason = disableBlockedReason(id);
        if (!reason.isEmpty()) { fail(reason); return; }
    }
    const auto found = records_.find(id);
    if (found == records_.end() || !found->second.error.isEmpty()) { fail(tr("插件管理工具不可用，请重新安装该插件后重试。")); return; }
    const auto manifest = found->second.manifest;
    DriverRemoval removal;
    if (operation == QStringLiteral("uninstall") && !manifest.management.contains(operation) && driverRemovalProvider_)
        removal = driverRemovalProvider_(providerId);
    const auto command = manifest.management.value(operation);
    const auto program = removal ? QString() : containedFile(manifest.directory, command.file);
    if (!removal && program.isEmpty()) { fail(tr("插件管理工具缺失或路径无效，已保留插件文件。")); return; }
    suspendedRuntimes_.insert(id);
    if (removeAfter) setEnabled(id, false, false);
    else stopPlugin(id, QStringLiteral("management"));
    emit managementStarting(providerId, operation);
    if (configWriter_ && !configWriter_()) {
        suspendedRuntimes_.remove(id);
        reload(); fail(tr("无法保存驱动管理状态，未执行系统驱动操作。"));
        emit managementFinished(providerId, operation, false); return;
    }
    const auto result = std::make_shared<ManagementResult>();
    const auto directory = manifest.directory;
    auto* thread = QThread::create([program, directory, command, manifest, removal = std::move(removal), result] {
        try {
            if (!verifyManifestContents(manifest, &result->message)) return;
            if (removal) { *result = removal(); return; }
            QProcess process;
            process.setProgram(program);
            process.setArguments(command.arguments);
            process.setWorkingDirectory(directory);
            process.setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments* args) { args->flags |= CREATE_NO_WINDOW; });
            process.setProcessChannelMode(QProcess::MergedChannels);
            process.start();
            if (!process.waitForStarted(5000)) { result->message = process.errorString(); return; }
            if (!process.waitForFinished(120000)) {
                process.kill(); process.waitForFinished(3000);
                result->message = QStringLiteral("插件管理操作超时，请检查实际系统状态。"); return;
            }
            result->message = QString::fromUtf8(process.readAll().left(8192));
            result->restartRequired = process.exitStatus() == QProcess::NormalExit && process.exitCode() == ERROR_SUCCESS_REBOOT_REQUIRED;
            result->ok = process.exitStatus() == QProcess::NormalExit && (process.exitCode() == 0 || result->restartRequired);
        } catch (const std::exception& error) { result->message = QString::fromUtf8(error.what()).left(8192); }
        catch (...) { result->message = QStringLiteral("插件管理操作发生异常。"); }
    });
    worker_ = thread;
    ++nativeDialogDepth_; emit dialogStateChanged(true);
    model_.setStatus(tr("正在执行插件管理操作…"), true);
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    connect(thread, &QThread::finished, this, [this, providerId, id, operation, removeAfter, result] {
        worker_ = nullptr;
        --nativeDialogDepth_; emit dialogStateChanged(pageDialogOpen_ || nativeDialogDepth_ > 0);
        suspendedRuntimes_.remove(id);
        if (removeAfter && result->ok && !result->restartRequired) completeRemoval(id);
        else {
            if (removeAfter) {
                auto& preference = config_.plugins[id.toStdString()];
                preference.removalPendingRestart = result->ok && result->restartRequired;
                preference.removalError = (preference.removalPendingRestart
                    ? tr("系统驱动卸载需要重启电脑；插件和工具已保留，重启电脑后点击“继续卸载”。")
                    : tr("系统驱动卸载未完成，插件和工具已保留，可重试或只移除插件。%1").arg(result->message.isEmpty() ? QString() : QLatin1Char('\n') + result->message)).left(2000).toStdString();
                emit saveRequested();
            } else if (result->ok && operation != QStringLiteral("uninstall")) {
                config_.plugins[id.toStdString()].removalError.clear();
                emit saveRequested();
            }
            reload();
            model_.setStatus(result->ok ? result->restartRequired
                ? tr("系统驱动卸载已提交，请重启电脑后继续。") : tr("管理操作已完成，可以主动重新连接驱动。")
                : tr("插件管理失败，文件已保留：%1").arg(result->message));
        }
        emit managementFinished(providerId, operation, result->ok);
    });
    thread->start();
}

void PluginService::updateModel() {
    QVariantList installed, discover, pages, builtin;
    for (const auto& [id, record] : records_) {
        auto row = record.manifest.presentation();
        const auto metadata = catalogPlugin(catalog_, id);
        const auto exact = catalogVersion(catalog_, id, record.manifest.version);
        const bool verifiedPackage = record.contentVerified && !record.integrityError;
        const bool bundledPackage = record.origin == QStringLiteral("bundled") && !record.invalid;
        const auto latest = catalogVersion(catalog_, id);
        row.insert("enabled", record.enabled);
        row.insert("pending", record.pending);
        row.insert("error", record.error);
        row.insert("blocked", !record.error.isEmpty());
        row.insert("invalid", record.invalid);
        const auto& preference = config_.plugins.at(id.toStdString());
        row.insert("installed", !preference.removed);
        row.insert("removalPendingRestart", preference.removalPendingRestart);
        row.insert("removalError", QString::fromStdString(preference.removalError));
        row.insert("canEnable", !preference.removalPendingRestart);
        const auto disableReason = preference.removed ? QString() : disableBlockedReason(id);
        row.insert("canDisable", !preference.removed && disableReason.isEmpty());
        row.insert("disableReason", disableReason);
        row.insert("canRemove", !preference.removed && disableReason.isEmpty());
        const auto providerId = record.manifest.drivers.isEmpty() ? id : record.manifest.providerId(record.manifest.drivers.front().id);
        row.insert("hasManagement", hasManagement(providerId, QStringLiteral("repair")));
        row.insert("hasSystemDriverUninstall", hasManagement(providerId, QStringLiteral("uninstall")));
        const auto bundled = bundledManifests_.find(id);
        const bool hasBundled = bundled != bundledManifests_.end();
        row.insert("bundledVersion", hasBundled ? bundled->second.version : QString());
        row.insert("canRestoreBundled", hasBundled && !record.pending && !preference.removalPendingRestart
            && (preference.removed || record.origin != QStringLiteral("bundled") || record.manifest.version != bundled->second.version));
        row.insert("bundled", bundledPackage);
        row.insert("official", bundledPackage || (verifiedPackage && metadata.value(QStringLiteral("official")).toBool()));
        row.insert("reviewed", verifiedPackage && exact.value(QStringLiteral("reviewed")).toBool());
        row.insert("publisherVerified", verifiedPackage);
        row.insert("publisher", verifiedPackage ? metadata.value(QStringLiteral("publisher")).toString() : QString());
        row.insert("source", bundledPackage ? tr("随程序提供") : record.origin == QStringLiteral("catalog") ? tr("插件中心") : tr("本地导入"));
        row.insert("repository", verifiedPackage ? metadata.value(QStringLiteral("repository")).toString() : QString());
        row.insert("integrityError", record.integrityError);
        row.insert("repairRequiresRestart", record.integrityError && NativeModule::isLoadedDirectory(record.manifest.directory));
        row.insert("canRepair", record.origin != QStringLiteral("bundled") && metadata.value(QStringLiteral("listed")).toBool()
            && validSha256(exact.value(QStringLiteral("contentsSha256")).toString())
            && compareVersions(QString::fromLatin1(APP_VERSION), exact.value(QStringLiteral("minHostVersion")).toString()) >= 0
            && (exact.value(QStringLiteral("maxHostVersion")).toString().isEmpty()
                || compareVersions(QString::fromLatin1(APP_VERSION), exact.value(QStringLiteral("maxHostVersion")).toString()) <= 0)
            && blockedReason(catalog_, id, record.manifest.version, exact.value(QStringLiteral("sha256")).toString()).isEmpty());
        row.insert("installationSource", record.source);
        row.insert("latestVersion", latest.value(QStringLiteral("version")).toString());
        row.insert("latestReviewed", latest.value(QStringLiteral("reviewed")).toBool());
        row.insert("notes", latest.value(QStringLiteral("notes")).toString());
        row.insert("updateAvailable", !preference.removed && !record.pending && !preference.removalPendingRestart && metadata.value(QStringLiteral("listed")).toBool()
            && validSha256(latest.value(QStringLiteral("contentsSha256")).toString())
            && !latest.isEmpty() && compareVersions(latest.value(QStringLiteral("version")).toString(), record.manifest.version) > 0);
        row.insert("currentDriver", QString::fromStdString(config_.activeDriverId).startsWith(id + QLatin1Char('/')));
        row.insert("backgroundEnabled", config_.plugins.at(id.toStdString()).backgroundEnabled);
        const auto runtime = runtimes_.find(id);
        if (runtime != runtimes_.end()) {
            const auto state = runtime->second->presentation();
            row.insert("runtimeStatus", state.value("status"));
            row.insert("backgroundRunning", state.value("backgroundRunning"));
            row.insert("runtimeError", state.value("error"));
            row.insert("windows", state.value("windows"));
            row.insert("commands", state.value("commands"));
        }
        if (record.manifest.builtin) builtin.append(row);
        if (!preference.removed) installed.append(row);
        if (record.enabled && record.error.isEmpty() && !suspendedRuntimes_.contains(id)
            && (!record.pending || !preference.activeVersion.empty()) && (record.manifest.native.isEmpty() || !preference.activeVersion.empty())) {
            for (const auto& page : record.manifest.pages) pages.append(QVariantMap{{"id", record.manifest.pageId(page.id)},
                {"pluginId", id}, {"title", page.title}, {"icon", page.iconFile.isEmpty() ? page.icon : QmlIconProvider::pageIconName(page.iconFile)},
                {"source", QUrl::fromLocalFile(containedFile(record.manifest.directory, page.file))}});
        }
    }
    for (const auto value : catalog_.value(QStringLiteral("plugins")).toArray()) {
        const auto record = value.toObject();
        if (!record.value(QStringLiteral("listed")).toBool()) continue;
        const auto id = record.value(QStringLiteral("id")).toString();
        const auto latest = catalogVersion(catalog_, id);
        if (latest.isEmpty() || !validSha256(latest.value(QStringLiteral("contentsSha256")).toString())) continue;
        auto row = record.toVariantMap();
        row.insert("version", latest.value(QStringLiteral("version")).toString());
        row.insert("reviewed", latest.value(QStringLiteral("reviewed")).toBool());
        row.insert("publisherVerified", true);
        row.insert("bundled", false);
        const auto preference = config_.plugins.find(id.toStdString());
        row.insert("installed", records_.contains(id) && preference != config_.plugins.end() && !preference->second.removed);
        row.insert("compatible", !latest.isEmpty() && validSha256(latest.value(QStringLiteral("contentsSha256")).toString()));
        row.insert("minHostVersion", latest.value(QStringLiteral("minHostVersion")).toString());
        row.insert("maxHostVersion", latest.value(QStringLiteral("maxHostVersion")).toString());
        row.insert("notes", latest.value(QStringLiteral("notes")).toString());
        row.insert("source", tr("插件中心"));
        row.insert("error", latest.isEmpty() ? tr("暂无兼容且可安装的版本")
            : !validSha256(latest.value(QStringLiteral("contentsSha256")).toString()) ? tr("此版本缺少可信文件校验信息，请刷新插件目录。") : QString());
        discover.append(row);
    }
    std::stable_sort(installed.begin(), installed.end(), [](const QVariant& left, const QVariant& right) { return driverOrder(left) < driverOrder(right); });
    std::stable_sort(builtin.begin(), builtin.end(), [](const QVariant& left, const QVariant& right) { return driverOrder(left) < driverOrder(right); });
    model_.setRegistryConfigured(!config_.pluginRegistryUrl.empty() && !config_.pluginRegistryPublicKey.empty());
    model_.setSnapshot(installed, discover, pages, builtin);
}

void PluginService::setRuntimeState(const QVariantMap& state) {
    if (runtimeState_ == state) return;
    runtimeState_ = state;
    for (auto& [id, context] : contexts_) context->setRuntimeState(state);
}
void PluginService::refreshTheme() { for (auto& [id, context] : contexts_) context->refreshTheme(); }

bool PluginService::verifyRecordContents(const QString& id, QString* error) {
    const auto found = records_.find(id);
    if (found == records_.end() || !found->second.error.isEmpty()) {
        if (error) *error = found == records_.end() ? tr("插件不可用。") : found->second.error;
        return false;
    }
    QString failure;
    if (verifyManifestContents(found->second.manifest, &failure)) return true;
    auto& record = found->second;
    record.error = failure;
    record.invalid = true;
    record.integrityError = true;
    record.contentVerified = false;
    if (error) *error = failure;
    QTimer::singleShot(0, this, [this, id] {
        const auto current = records_.find(id);
        if (current == records_.end() || !current->second.integrityError) return;
        stopPlugin(id, QStringLiteral("integrity"));
        updateModel(); emit inventoryChanged();
    });
    return false;
}

PluginPageContext* PluginService::contextForPage(const QString& id) {
    const auto descriptor = model_.page(id);
    const auto pluginId = descriptor.value("pluginId").toString();
    return contextForPlugin(pluginId);
}

PluginPageContext* PluginService::contextForPlugin(const QString& pluginId) {
    const auto found = records_.find(pluginId);
    if (shuttingDown_ || suspendedRuntimes_.contains(pluginId) || found == records_.end() || !found->second.enabled || !found->second.error.isEmpty()) return nullptr;
    const auto existing = contexts_.find(pluginId);
    if (existing != contexts_.end()) return existing->second;
    if (!verifyRecordContents(pluginId, nullptr)) return nullptr;
    auto* context = new PluginPageContext(pluginId, found->second.manifest.permissions, this);
    context->setContentsVerifier([this, pluginId](QString* error) { return verifyRecordContents(pluginId, error); });
    context->setProperty("pluginVersion", found->second.manifest.version);
    context->setProperty("pluginDirectory", found->second.manifest.directory);
    context->setRuntimeState(runtimeState_);
    context->setSettings(QJsonDocument::fromJson(QByteArray::fromStdString(config_.plugins[pluginId.toStdString()].settingsJson)).object().toVariantMap());
    connect(context, &PluginPageContext::settingEdited, this, [this, pluginId, context](const QString&, const QVariant&) {
        config_.plugins[pluginId.toStdString()].settingsJson = QJsonDocument::fromVariant(context->settings()).toJson(QJsonDocument::Compact).toStdString();
        emit saveRequested();
    });
    connect(context, &PluginPageContext::navigationRequested, this, [this, pluginId](const QString& page) {
        const auto record = records_.find(pluginId);
        if (shuttingDown_ || record == records_.end() || !record->second.enabled || !record->second.error.isEmpty()) return;
        const QStringList core{QStringLiteral("keys"), QStringLiteral("macro"), QStringLiteral("plugins"), QStringLiteral("driver"), QStringLiteral("settings"), QStringLiteral("about")};
        if (core.contains(page) || (!model_.page(page).isEmpty() && model_.page(page).value("pluginId").toString() == pluginId)) emit navigationRequested(page);
    });
    connect(context, &PluginPageContext::engineRequested, this, [this, pluginId, context](bool macro, bool running) {
        const auto record = records_.find(pluginId);
        if (shuttingDown_ || busy() || record == records_.end() || !record->second.enabled || !record->second.error.isEmpty()
            || !contexts_.contains(pluginId) || contexts_.at(pluginId) != context) return;
        if (running && !confirm(tr("插件请求启动"), tr("%1 请求启动%2，将使用当前驱动发送配置的按键。")
            .arg(record->second.manifest.name, macro ? tr("循环连发") : tr("按键连发")), tr("启动"))) return;
        const auto current = records_.find(pluginId);
        if (shuttingDown_ || current == records_.end() || !current->second.enabled || !current->second.error.isEmpty()
            || !contexts_.contains(pluginId) || contexts_.at(pluginId) != context) return;
        emit engineRequested(macro, running);
    }, Qt::QueuedConnection);
    connect(context, &PluginPageContext::logRequested, this, [pluginId](const QString& text) { DiagLog::Write("plugin[" + pluginId.toStdString() + "] " + text.toStdString()); });
    connect(context, &PluginPageContext::messageRequested, this, [this, pluginId, context](const QString& title, const QString& message) {
        const auto record = records_.find(pluginId);
        if (shuttingDown_ || busy() || record == records_.end() || !record->second.enabled || !record->second.error.isEmpty()
            || !contexts_.contains(pluginId) || contexts_.at(pluginId) != context) return;
        const DialogGuard guard(*this);
        AppMessageDialog::information(window_, title, tr("来自插件：%1\n\n%2").arg(record->second.manifest.name, message));
    }, Qt::QueuedConnection);
    connect(context, &PluginPageContext::confirmationRequested, this, [this, pluginId, context = QPointer<PluginPageContext>(context)](const QString& requestId, const QString& title, const QString& message) {
        const auto record = records_.find(pluginId);
        if (!context) return;
        const bool accepted = !shuttingDown_ && !busy() && record != records_.end() && record->second.enabled && record->second.error.isEmpty()
            && contexts_.contains(pluginId) && contexts_.at(pluginId) == context.data()
            && confirm(title, tr("来自插件：%1\n\n%2").arg(record->second.manifest.name, message), tr("确定"));
        if (context) emit context->confirmationFinished(requestId, accepted);
    }, Qt::QueuedConnection);
    contexts_.emplace(pluginId, context);
    return context;
}

void PluginService::syncRuntimes() {
    if (shuttingDown_) return;
    QStringList resume;
    for (const auto& [id, record] : records_) {
        const auto& preference = config_.plugins.at(id.toStdString());
        if (!record.enabled || !record.error.isEmpty() || suspendedRuntimes_.contains(id) || runtimes_.contains(id)
            || (record.pending && preference.activeVersion.empty())
            || (!record.manifest.native.isEmpty() && preference.activeVersion.empty())) continue;
        auto* context = contextForPlugin(id);
        if (!context) continue;
        auto instance = std::make_unique<PluginRuntime>(record.manifest, context,
            QJsonDocument::fromJson(QByteArray::fromStdString(preference.windowStateJson)).object().toVariantMap(), this);
        auto* runtime = instance.get();
        connect(runtime, &PluginRuntime::changed, this, &PluginService::updateModel);
        connect(runtime, &PluginRuntime::windowStateSaved, this, [this, id](const QVariantMap& state) {
            auto& preference = config_.plugins[id.toStdString()];
            const auto json = QJsonDocument::fromVariant(state).toJson(QJsonDocument::Compact).toStdString();
            if (json != preference.windowStateJson) { preference.windowStateJson = json; emit saveRequested(); }
        });
        connect(runtime, &PluginRuntime::backgroundRequested, this, [this, id](bool running) { setBackgroundRunning(id, running); });
        connect(runtime, &PluginRuntime::backgroundFailed, runtime, [this, id, runtime](const QString& error) {
            config_.plugins[id.toStdString()].backgroundEnabled = false;
            emit saveRequested(); updateModel();
            model_.setStatus(tr("插件后台启动失败：%1").arg(error));
            const auto found = records_.find(id);
            if (found != records_.end() && found->second.manifest.backgroundNative)
                QTimer::singleShot(0, runtime, [this, id] { (void)rollbackActivation(id, QStringLiteral("background")); });
        });
        connect(runtime, &PluginRuntime::backgroundStarted, runtime, [this, id] { confirmActivation(id, QStringLiteral("background")); });
        const bool nativeCommands = std::any_of(record.manifest.commands.cbegin(), record.manifest.commands.cend(), [](const auto& command) {
            return command.target == QStringLiteral("native");
        });
        if (record.manifest.backgroundNative || nativeCommands) {
            auto* backend = new PluginNativeBackend(record.manifest, hostDirectory_, runtime);
            connect(runtime, &PluginRuntime::nativeStartRequested, this, [this, id, runtime, backend] {
                QString error;
                if (!prepareActivation(id, QStringLiteral("background"), &error)) runtime->nativeStarted(false, error);
                else backend->start();
            });
            connect(runtime, &PluginRuntime::nativeStopRequested, backend, &PluginNativeBackend::stop);
            connect(runtime, &PluginRuntime::nativeRequestEnded, backend, &PluginNativeBackend::cancelRequest);
            connect(runtime, &PluginRuntime::nativeRequestIssued, this, [this, id, runtime, backend, nativeBackground = record.manifest.backgroundNative](const QString& requestId, const QString& command, const QVariantMap& payload) {
                QString error;
                if (!prepareActivation(id, nativeBackground ? QStringLiteral("background") : QStringLiteral("backend"), &error)) runtime->nativeReply(requestId, false, {}, error);
                else backend->request(requestId, command, payload);
            });
            connect(backend, &PluginNativeBackend::started, runtime, &PluginRuntime::nativeStarted, Qt::QueuedConnection);
            connect(backend, &PluginNativeBackend::stopped, runtime, &PluginRuntime::nativeStopped, Qt::QueuedConnection);
            connect(backend, &PluginNativeBackend::stopped, runtime, [this, id, runtime] {
                if (runtime->presentation().value("status").toString() == QStringLiteral("stopped")) cancelActivations(id, true);
            }, Qt::QueuedConnection);
            connect(backend, &PluginNativeBackend::replied, runtime, &PluginRuntime::nativeReply);
            connect(backend, &PluginNativeBackend::published, runtime, &PluginRuntime::nativeEvent);
            if (!record.manifest.backgroundNative)
                connect(backend, &PluginNativeBackend::initialized, runtime, [this, id, runtime](bool ok, const QString&) {
                    if (ok) confirmActivation(id, QStringLiteral("backend"));
                    else QTimer::singleShot(0, runtime, [this, id] { (void)rollbackActivation(id, QStringLiteral("backend")); });
                }, Qt::QueuedConnection);
        }
        runtimes_.emplace(id, std::move(instance));
        if (preference.backgroundEnabled && record.manifest.hasBackground()) resume.append(id);
    }
    // Start after the inventory is complete; startup code can use its own context and windows.
    for (const auto& id : resume) QTimer::singleShot(0, this, [this, id] {
        const auto runtime = runtimes_.find(id);
        if (runtime != runtimes_.end() && config_.plugins[id.toStdString()].backgroundEnabled)
            runtime->second->startBackground(QStringLiteral("restore"));
    });
}

void PluginService::setBackgroundRunning(const QString& id, bool running) {
    const auto found = runtimes_.find(id);
    if (found == runtimes_.end() || !records_.at(id).manifest.hasBackground()) return;
    const auto status = found->second->presentation().value("status").toString();
    if (status == QStringLiteral("starting") || status == QStringLiteral("stopping")) return;
    config_.plugins[id.toStdString()].backgroundEnabled = running;
    emit saveRequested();
    if (running) found->second->startBackground();
    else found->second->stopBackground();
    updateModel();
}

void PluginService::fail(const QString& message) { model_.setStatus(message, hostBusy_ || reply_ || worker_); }

void PluginService::download(const QString& url, qsizetype maximum, std::function<void(const QByteArray&)> completed,
    std::function<void(const QString&)> failed) {
    if (reply_ || worker_) { fail(tr("已有插件操作正在进行，请稍后重试。")); return; }
    if (!safeUrl(url)) {
        const auto message = tr("插件下载需要无认证信息的 HTTPS 地址。");
        if (failed) failed(message); else fail(message);
        return;
    }
    transfer_.clear();
    QNetworkRequest request{QUrl(url)};
    request.setRawHeader("User-Agent", "XiaoJiAutoKey/" APP_VERSION);
    auto* reply = network_.get(request);
    reply_ = reply;
    model_.setStatus(catalogRefreshing_ ? tr("正在获取插件列表…") : tr("正在下载插件…"), true);
    connect(reply, &QIODevice::readyRead, this, [this, reply, maximum] {
        const auto bytes = reply->readAll();
        if (bytes.size() > maximum - transfer_.size()) { reply->setProperty("sizeExceeded", true); reply->abort(); return; }
        transfer_.append(bytes);
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply, maximum, completed = std::move(completed), failed = std::move(failed)] {
        const auto final = reply->readAll();
        if (final.size() <= maximum - transfer_.size()) transfer_.append(final);
        else reply->setProperty("sizeExceeded", true);
        const auto bytes = std::move(transfer_);
        const bool ok = !reply->property("sizeExceeded").toBool() && reply->error() == QNetworkReply::NoError
            && reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 200 && safeUrl(reply->url().toString());
        const QString message = reply->property("sizeExceeded").toBool() ? tr("插件下载超过大小限制。") : reply->errorString();
        reply_ = nullptr;
        reply->deleteLater();
        if (shuttingDown_) return;
        if (!ok) {
            if (failed) failed(message);
            else {
                DiagLog::Write("plugin download: " + message.toStdString());
                fail(tr("插件下载失败，请稍后重试。"));
            }
            return;
        }
        model_.setStatus({}, false);
        completed(bytes);
    });
    QTimer::singleShot(120000, reply, [reply] { if (!reply->isFinished()) reply->abort(); });
}

void PluginService::refreshBackgroundChecks() {
    if (config_.automaticUpdateChecks) QTimer::singleShot(2500, this, [this] { if (config_.automaticUpdateChecks) refreshCatalog(); });
}

void PluginService::refreshCatalog(bool manual) {
    if (shuttingDown_ || catalogRefreshing_) return;
    if (busy()) {
        if (manual) catalogRefreshPending_ = true;
        return;
    }
    catalogRefreshPending_ = false;
    if (config_.pluginRegistryUrl.empty() || config_.pluginRegistryPublicKey.empty()) {
        if (manual) finishCatalogRefresh(tr("当前发行版尚未接通插件目录，可先通过“导入”安装本地插件。"));
        return;
    }
    const auto key = QByteArray::fromBase64(QByteArray::fromStdString(config_.pluginRegistryPublicKey));
    const auto primary = QString::fromStdString(config_.pluginRegistryUrl);
    QStringList sources{primary};
    const auto fallback = QString::fromStdString(config_.pluginRegistryFallbackUrl);
    if (!fallback.isEmpty()) sources.append(fallback);
    sources.removeDuplicates();
    catalogRefreshing_ = true;
    model_.setCatalogRefreshState(true);
    tryCatalogSources(sources, key, primary);
}

void PluginService::refreshPendingCatalog() {
    if (shuttingDown_ || !catalogRefreshPending_ || busy()) return;
    refreshCatalog(true);
}

void PluginService::finishCatalogRefresh(const QString& error) {
    catalogRefreshing_ = false;
    model_.setCatalogRefreshState(false, error);
    if (!error.isEmpty()) DiagLog::Write("plugin catalog: " + error.toStdString());
    model_.setStatus({}, busy());
}

void PluginService::tryCatalogSources(QStringList sources, const QByteArray& key, const QString& primary, const QString& lastError) {
    if (primary != QString::fromStdString(config_.pluginRegistryUrl)) { finishCatalogRefresh(); return; }
    if (sources.isEmpty()) {
        finishCatalogRefresh(lastError);
        return;
    }
    const auto source = sources.takeFirst();
    download(source, 8 * 1024 * 1024, [this, key, primary, sources](const QByteArray& bytes) {
        if (primary != QString::fromStdString(config_.pluginRegistryUrl)) { finishCatalogRefresh(); return; }
        QJsonObject catalog;
        QString error;
        if (!decodeCatalog(bytes, key, catalog_.value(QStringLiteral("revision")).toVariant().toLongLong(), &catalog, &error)) {
            tryCatalogSources(sources, key, primary, error); return;
        }
        if (!writeBytes(catalogCachePath(), bytes)) { finishCatalogRefresh(tr("无法保存插件目录缓存。")); return; }
        catalog_ = catalog;
        catalogEnvelope_ = bytes;
        for (const auto& [id, record] : records_) {
            const auto reason = blockedReason(catalog_, id, record.manifest.version, record.hash);
            if (!reason.isEmpty() && record.enabled) {
                emit beforePluginDisabled(id);
                waitForHealthCheck();
                for (auto& [provider, driver] : drivers_) if (provider.startsWith(id + QLatin1Char('/'))) { driver->ReleaseAll(); driver->setEnabled(false); driver->disconnect(); }
            }
        }
        reload();
        finishCatalogRefresh();
    }, [this, sources, key, primary](const QString& error) { tryCatalogSources(sources, key, primary, error); });
}

void PluginService::downloadPackage(QStringList sources, const QString& id, const QString& hash, bool repair, const QString& lastError) {
    if (sources.isEmpty()) {
        DiagLog::Write("plugin download: " + lastError.toStdString());
        fail(tr("插件下载失败，请稍后重试。"));
        return;
    }
    const auto source = sources.takeFirst();
    download(source, 64 * 1024 * 1024, [this, sources, source, id, hash, repair](const QByteArray& bytes) {
        const auto actual = QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex());
        if (actual.compare(hash, Qt::CaseInsensitive) != 0) {
            downloadPackage(sources, id, hash, repair, tr("来源内容与签名目录中的包哈希不一致。")); return;
        }
        stage(bytes, source, id, hash, repair);
    }, [this, sources, id, hash, repair](const QString& error) {
        downloadPackage(sources, id, hash, repair, error);
    });
}

void PluginService::stage(const QByteArray& zip, const QString& source, const QString& expectedId, const QString& expectedHash, bool repair) {
    if (worker_ || reply_) { fail(tr("已有插件操作正在进行。")); return; }
    struct Result { StagedPackage package; QString error; bool ok = false; };
    const auto result = std::make_shared<Result>();
    const auto cache = cacheDirectory_;
    auto* thread = QThread::create([zip, cache, result] {
        try { result->ok = stagePackage(zip, cache, &result->package, &result->error); }
        catch (const std::exception& exception) { result->error = QString::fromUtf8(exception.what()); }
        catch (...) { result->error = QStringLiteral("插件包处理失败。"); }
    });
    worker_ = thread;
    model_.setStatus(tr("正在校验插件包…"), true);
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    connect(thread, &QThread::finished, this, [this, result, source, expectedId, expectedHash, repair] {
        worker_ = nullptr;
        if (!result->ok) { fail(result->error); return; }
        const auto& package = result->package;
        if ((!expectedId.isEmpty() && expectedId != package.manifest.id)
            || (!expectedHash.isEmpty() && expectedHash.compare(package.sha256, Qt::CaseInsensitive) != 0)) {
            QDir(package.directory).removeRecursively(); fail(tr("插件包的身份或内容与目录不一致。")); return;
        }
        const auto metadata = catalogVersion(catalog_, package.manifest.id, package.manifest.version);
        const bool matchesCatalog = metadata.value(QStringLiteral("sha256")).toString().compare(package.sha256, Qt::CaseInsensitive) == 0;
        const bool verified = matchesCatalog && validSha256(metadata.value(QStringLiteral("contentsSha256")).toString())
            && metadata.value(QStringLiteral("contentsSha256")).toString().compare(package.contentsSha256, Qt::CaseInsensitive) == 0;
        if ((matchesCatalog || !expectedId.isEmpty()) && !verified) {
            QDir(package.directory).removeRecursively();
            fail(tr("插件目录缺少有效的内容校验信息或与包内容不一致，请刷新目录后重试。")); return;
        }
        const auto listed = catalogPlugin(catalog_, package.manifest.id);
        const auto reason = blockedReason(catalog_, package.manifest.id, package.manifest.version, package.sha256);
        if (!reason.isEmpty() || (!listed.isEmpty() && !listed.value(QStringLiteral("listed")).toBool())
            || (package.manifest.id.startsWith(QStringLiteral("xiaoji.")) && (!verified || !listed.value(QStringLiteral("official")).toBool()))) {
            QDir(package.directory).removeRecursively();
            fail(reason.isEmpty() ? tr("此插件已下架或不能验证为官方发布。") : reason); return;
        }
        model_.setStatus({}, false);
        const bool reviewed = verified && metadata.value(QStringLiteral("reviewed")).toBool();
        const auto publisher = verified ? listed.value(QStringLiteral("publisher")).toString() : tr("发布者未验证");
        QString details = tr("%1\n版本：%2\n发布者：%3\n作者自述：%4\n安装来源：%5\n审核：%6")
            .arg(package.manifest.name, package.manifest.version, publisher, package.manifest.author, source,
                reviewed ? tr("该版本审核通过") : tr("未审核"));
        if (verified && !listed.value(QStringLiteral("repository")).toString().isEmpty())
            details += tr("\n发布仓库：%1").arg(listed.value(QStringLiteral("repository")).toString());
        const auto previous = records_.find(package.manifest.id);
        bool sourceChanged = false;
        if (previous != records_.end()) {
            const auto& old = previous->second;
            const auto oldMetadata = catalogPlugin(catalog_, old.manifest.id);
            const auto oldInfo = readObject(QDir(old.manifest.directory).filePath(QStringLiteral(".installation.json")));
            const auto oldPublisher = old.contentVerified ? oldMetadata.value(QStringLiteral("publisher")).toString()
                : oldInfo.value(QStringLiteral("publisher")).toString();
            const auto oldRepository = old.contentVerified ? oldMetadata.value(QStringLiteral("repository")).toString()
                : oldInfo.value(QStringLiteral("repository")).toString();
            sourceChanged = old.origin == QStringLiteral("bundled") ? !verified
                : oldPublisher != (verified ? listed.value(QStringLiteral("publisher")).toString() : QString())
                    || oldRepository.compare(verified ? listed.value(QStringLiteral("repository")).toString() : QString(), Qt::CaseInsensitive) != 0;
            if (!verified && oldPublisher.isEmpty()) sourceChanged = old.source != source;
            if (sourceChanged) details += tr("\n\n将更换此插件的发布来源。\n原发布者：%1\n新发布者：%2\n原来源：%3\n新来源：%4\n相同插件 ID 不代表相同发布者，原有插件设置将保留。")
                .arg(oldPublisher.isEmpty() ? old.origin == QStringLiteral("bundled") ? tr("随程序提供") : tr("发布者未验证") : oldPublisher,
                    publisher, oldRepository.isEmpty() ? old.source : oldRepository,
                    verified ? listed.value(QStringLiteral("repository")).toString() : source);
        }
        details += tr("\n\n作者自述不作为身份认证。插件将在你的电脑上运行，请确认你信任上述发布来源。");
        if (confirm(sourceChanged ? tr("更换插件发布来源") : repair ? tr("重新安装插件") : tr("安装插件"), details,
                sourceChanged ? tr("更换来源并安装") : repair ? tr("重新安装") : tr("安装")))
            installStaged(package.directory, package.sha256, package.manifest, source, !expectedId.isEmpty(), repair);
        else QDir(package.directory).removeRecursively();
    });
    thread->start();
}

void PluginService::installStaged(const QString& directory, const QString& hash, const Manifest& manifest, const QString& source, bool fromCatalog, bool repair) {
    const auto preferenceFound = config_.plugins.find(manifest.id.toStdString());
    if (preferenceFound != config_.plugins.end() && preferenceFound->second.removalPendingRestart) {
        QDir(directory).removeRecursively(); fail(tr("此插件的系统驱动卸载尚未完成，请先重启电脑并继续卸载。")); return;
    }
    const auto metadata = catalogVersion(catalog_, manifest.id, manifest.version);
    const bool catalogPackage = metadata.value(QStringLiteral("sha256")).toString().compare(hash, Qt::CaseInsensitive) == 0
        && metadata.value(QStringLiteral("contentsSha256")).toString().compare(manifest.contentsSha256, Qt::CaseInsensitive) == 0;
    const auto plugin = catalogPlugin(catalog_, manifest.id);
    if ((fromCatalog && !catalogPackage) || (manifest.id.startsWith(QStringLiteral("xiaoji."))
            && (!catalogPackage || !plugin.value(QStringLiteral("official")).toBool()))) {
        QDir(directory).removeRecursively(); fail(tr("插件发布身份已变化，请刷新目录并重新安装。")); return;
    }
    QString integrityError;
    if (!verifyManifestContents(manifest, &integrityError)) {
        QDir(directory).removeRecursively(); fail(integrityError); return;
    }
    const auto parent = containedFile(hostDirectory_, QStringLiteral("plugins/installed/") + manifest.id, false);
    if (parent.isEmpty() || QFileInfo(parent).isSymLink() || QFileInfo(parent).isJunction()) {
        QDir(directory).removeRecursively(); fail(tr("插件安装目录无效或包含链接。")); return;
    }
    const auto target = QDir(parent).filePath(manifest.version);
    if (!QDir().mkpath(parent)) { QDir(directory).removeRecursively(); fail(tr("无法创建插件安装目录。")); return; }
    const QJsonObject info{{QStringLiteral("sha256"), hash}, {QStringLiteral("contentsSha256"), manifest.contentsSha256},
        {QStringLiteral("origin"), fromCatalog ? QStringLiteral("catalog") : QStringLiteral("local")}, {QStringLiteral("source"), source},
        {QStringLiteral("publisher"), catalogPackage ? plugin.value(QStringLiteral("publisher")).toString() : QString()},
        {QStringLiteral("repository"), catalogPackage ? plugin.value(QStringLiteral("repository")).toString() : QString()}};
    if (!writeBytes(QDir(directory).filePath(QStringLiteral(".installation.json")), QJsonDocument(info).toJson())) {
        QDir(directory).removeRecursively(); fail(tr("无法保存插件安装记录。")); return;
    }
    if (QFileInfo::exists(target)) {
        const QFileInfo targetInfo(target);
        if (targetInfo.isSymLink() || targetInfo.isJunction()) {
            QDir(directory).removeRecursively(); fail(tr("现有插件版本目录包含链接，不能替换。")); return;
        }
        const auto current = readObject(QDir(target).filePath(QStringLiteral(".installation.json")));
        if (current.value(QStringLiteral("sha256")).toString().compare(hash, Qt::CaseInsensitive) != 0 && !(repair && catalogPackage)) {
            QDir(directory).removeRecursively(); fail(tr("同一插件版本已经存在不同内容，不能覆盖。")); return;
        }
        auto installedManifest = manifest;
        installedManifest.directory = target;
        if (verifyManifestContents(installedManifest, nullptr)) {
            QDir(directory).removeRecursively();
            if (!writeBytes(QDir(target).filePath(QStringLiteral(".installation.json")), QJsonDocument(info).toJson())) {
                fail(tr("无法更新插件安装记录。")); return;
            }
        } else {
            if (NativeModule::isLoadedDirectory(target)) {
                QDir(directory).removeRecursively(); fail(tr("此版本的原生模块仍在使用，请重启小叽后再重新安装。")); return;
            }
            stopPlugin(manifest.id, QStringLiteral("repair"));
            const auto backup = QDir(parent).filePath(QStringLiteral(".replaced-") + QUuid::createUuid().toString(QUuid::WithoutBraces));
            if (!QDir().rename(target, backup)) {
                QDir(directory).removeRecursively(); reload(); fail(tr("无法保留现有插件文件，未执行替换。")); return;
            }
            if (!QDir().rename(directory, target)) {
                const bool restored = QDir().rename(backup, target);
                QDir(directory).removeRecursively(); reload();
                fail(restored ? tr("无法替换插件，已恢复原有文件。") : tr("无法替换插件，原有文件保留在插件备份目录中。")); return;
            }
            if (!QDir(backup).removeRecursively()) DiagLog::Write("plugin repair: old files kept at " + backup.toStdString());
        }
    } else {
        if (!QDir().rename(directory, target)) { QDir(directory).removeRecursively(); fail(tr("无法原子安装插件文件。")); return; }
    }
    const auto previous = records_.find(manifest.id);
    const bool requiresRestart = !manifest.native.isEmpty()
        || (previous != records_.end() && !previous->second.manifest.native.isEmpty()) || NativeModule::isLoaded(manifest.id);
    auto& preference = config_.plugins[manifest.id.toStdString()];
    if (previous == records_.end() || preference.removed || previous->second.invalid) { preference.enabled = true; preference.activeVersion.clear(); }
    preference.removed = false;
    if (requiresRestart) preference.pendingVersion = manifest.version.toStdString();
    else {
        preference.activeVersion = manifest.version.toStdString(); preference.pendingVersion.clear();
        preference.trialVersion.clear(); preference.verifiedActivations.clear(); preference.activationInProgress = false;
    }
    emit saveRequested();
    reload();
    model_.setStatus(requiresRestart ? tr("插件已安装，重启小叽后生效。") : tr("插件已安装；后台功能需主动启动。"));
}

QString PluginService::disableBlockedReason(const QString& id) const {
    return supportedBuiltin(id) && !hasAvailableBuiltin(id)
        ? tr("至少保留一个可用的内置驱动，请先安装并启用另一种内置驱动。") : QString();
}

bool PluginService::hasAvailableBuiltin(const QString& excluded) const {
    for (const auto& [id, record] : records_) {
        if (id == excluded || !supportedBuiltin(id)) continue;
        const auto preference = config_.plugins.find(id.toStdString());
        if (preference == config_.plugins.end() || preference->second.removed || preference->second.removalPendingRestart) continue;
        for (const auto& driver : record.manifest.drivers)
            if (available(record.manifest.providerId(driver.id))) return true;
    }
    return false;
}

bool PluginService::bundledManifest(const QString& id, Manifest* manifest) const {
    if (!manifest || !supportedBuiltin(id)) return false;
    const QDir base(QDir(hostDirectory_).filePath(QStringLiteral("plugins/builtin/")));
    const auto root = base.canonicalPath();
    const QDir directory(base.filePath(id));
    const auto pluginPath = directory.canonicalPath();
    if (root.isEmpty() || pluginPath.isEmpty() || !pluginPath.startsWith(root + QLatin1Char('/'), Qt::CaseInsensitive)) return false;
    bool found = false;
    int count = 0;
    for (const auto& folder : directory.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks)) {
        if (++count > 128 || !validVersion(folder.fileName())) continue;
        Manifest candidate;
        if (!readManifest(QDir(folder.absoluteFilePath()).filePath(QStringLiteral("plugin.json")), &candidate, nullptr)
            || candidate.id != id || candidate.version != folder.fileName()
            || !blockedReason(catalog_, id, candidate.version, {}).isEmpty()) continue;
        if (!found || compareVersions(candidate.version, manifest->version) > 0) {
            *manifest = std::move(candidate); found = true;
        }
    }
    return found;
}

void PluginService::restoreBundled(const QString& id) {
    const auto current = records_.find(id);
    Manifest bundled;
    if (current == records_.end() || !bundledManifest(id, &bundled)) { fail(tr("没有可恢复的内置驱动，请重新获取应用发布包。")); return; }
    auto& preference = config_.plugins[id.toStdString()];
    if (preference.removalPendingRestart) { fail(tr("请先重启电脑并继续完成卸载，再安装此驱动。")); return; }
    if (current->second.pending) { fail(tr("请先重启应用以完成已经准备的版本切换。")); return; }
    const bool reinstall = preference.removed;
    if (!confirm(reinstall ? tr("安装内置驱动") : tr("恢复随包版本"),
        tr("将安装随小叽提供的 %1 v%2，原来的设置会留下，重启小叽后生效。").arg(bundled.name, bundled.version), reinstall ? tr("安装") : tr("恢复"))) return;
    if (reinstall) preference.enabled = true;
    preference.removed = false;
    preference.removalError.clear();
    preference.pendingVersion = bundled.version.toStdString();
    emit saveRequested();
    reload();
    model_.setStatus(tr("已准备恢复 %1 v%2，重启小叽后生效。").arg(bundled.name, bundled.version));
}

void PluginService::setEnabled(const QString& id, bool enabled, bool ask) {
    const auto found = records_.find(id);
    if (found == records_.end()) return;
    if (enabled && !found->second.error.isEmpty()) { fail(found->second.error); updateModel(); return; }
    if (enabled && config_.plugins[id.toStdString()].removalPendingRestart) {
        fail(tr("请先重启电脑并继续完成系统驱动卸载，再启用或重新安装此插件。")); updateModel(); return;
    }
    if (!enabled) {
        const auto reason = disableBlockedReason(id);
        if (!reason.isEmpty()) { fail(reason); updateModel(); return; }
    }
    if (!enabled && ask && QString::fromStdString(config_.activeDriverId).startsWith(id + QLatin1Char('/'))
        && !confirm(tr("停用当前驱动"), tr("将停止连发和循环、释放按键，并选择其他可用驱动。需要由你重新连接后使用。"), tr("停用"))) {
        updateModel(); return;
    }
    if (!enabled) {
        stopPlugin(id, QStringLiteral("disabled"));
        if (QString::fromStdString(config_.activeDriverId).startsWith(id + QLatin1Char('/'))) config_.activeDriverId.clear();
    }
    config_.plugins[id.toStdString()].enabled = enabled;
    emit saveRequested();
    reload();
}

void PluginService::remove(const QString& id) {
    const auto found = records_.find(id);
    if (found == records_.end()) return;
    const auto reason = disableBlockedReason(id);
    if (!reason.isEmpty()) { fail(reason); return; }
    const auto manifest = found->second.manifest;
    const auto providerId = manifest.drivers.isEmpty() ? id : manifest.providerId(manifest.drivers.front().id);
    const bool hasUninstall = hasManagement(providerId, QStringLiteral("uninstall"));
    bool removeSystemDriver = hasUninstall;
    {
        const DialogGuard guard(*this);
        const auto message = hasUninstall
            ? tr("卸载 %1，移除插件及其页面入口，保留设置。勾选下方选项时，先清理此插件使用的专属系统驱动；可能需要重启电脑。Windows 自带驱动不会删除。%2")
                .arg(manifest.name, manifest.management.contains(QStringLiteral("uninstall")) ? QLatin1Char('\n') + manifest.management.value(QStringLiteral("uninstall")).confirmation : QString())
            : tr("卸载 %1，移除插件包及页面入口，保留插件设置。%2").arg(manifest.name,
                manifest.drivers.isEmpty() ? QString() : tr("此插件未提供系统驱动卸载能力，只移除插件文件；已有的 Windows 驱动保持不变。"));
        if (!ConfigConfirmDialog::confirmWithOption(window_, tr("卸载插件"), message, tr("卸载"),
            hasUninstall ? tr("同时卸载此插件使用的系统驱动") : QString(), &removeSystemDriver)) return;
    }
    const auto currentReason = disableBlockedReason(id);
    if (!currentReason.isEmpty()) { fail(currentReason); return; }
    if (removeSystemDriver && hasUninstall) { executeManagement(providerId, QStringLiteral("uninstall"), true); return; }
    setEnabled(id, false, false);
    if (config_.plugins[id.toStdString()].enabled) return;
    completeRemoval(id);
}

void PluginService::completeRemoval(const QString& id) {
    const auto reason = disableBlockedReason(id);
    if (!reason.isEmpty()) {
        config_.plugins[id.toStdString()].enabled = true;
        reload();
        fail(reason);
        return;
    }
    auto& preference = config_.plugins[id.toStdString()];
    const auto previous = preference;
    preference.removed = true;
    preference.activeVersion.clear(); preference.pendingVersion.clear(); preference.rollbackVersion.clear();
    preference.trialVersion.clear(); preference.verifiedActivations.clear(); preference.activationInProgress = false;
    preference.removalPendingRestart = false; preference.removalError.clear();
    if (configWriter_ && !configWriter_()) {
        preference = previous;
        reload(); fail(tr("无法保存插件卸载状态，插件文件已保留，请重试。")); return;
    }
    emit saveRequested();
    reload();
    model_.setStatus(tr("插件已卸载，文件将在重启时清理，设置已保留。"));
}

void PluginService::dispatch(const QString& action, const QString& id, const QVariant& value) {
    if (action == QStringLiteral("dialog")) { pageDialogOpen_ = value.toBool(); emit dialogStateChanged(pageDialogOpen_ || nativeDialogDepth_ > 0); return; }
    if (action == QStringLiteral("refresh")) { refreshCatalog(true); return; }
    if (action != QStringLiteral("open") && busy()) { fail(tr("已有插件操作正在进行，请稍后重试。")); return; }
    if (action == QStringLiteral("repository")) {
        const auto plugin = catalogPlugin(catalog_, id);
        const auto repository = plugin.value(QStringLiteral("repository")).toString();
        if (!repository.isEmpty()) QDesktopServices::openUrl(QUrl(repository));
        return;
    }
    if (action == QStringLiteral("enable")) { setEnabled(id, value.toBool()); return; }
    if (action == QStringLiteral("background")) { setBackgroundRunning(id, value.toBool()); return; }
    if (action == QStringLiteral("window") || action == QStringLiteral("window-close") || action == QStringLiteral("window-interactive")) {
        const auto runtime = runtimes_.find(id);
        if (runtime != runtimes_.end()) runtime->second->windowAction(action == QStringLiteral("window") ? QStringLiteral("show")
            : action == QStringLiteral("window-close") ? QStringLiteral("close") : QStringLiteral("configure"), value.toString(),
            action == QStringLiteral("window-interactive") ? QVariantMap{{"clickThrough", false}} : QVariantMap{});
        return;
    }
    if (action == QStringLiteral("command")) {
        if (!runtimes_.contains(id)) { fail(tr("插件运行实例不可用，请确认启用状态或重启应用。")); return; }
        if (auto* context = contextForPlugin(id)) {
            const auto requestId = context->request(value.toString(), {});
            if (requestId.isEmpty()) { fail(tr("插件命令不可用。")); return; }
            auto* observer = new QObject(context);
            connect(context, &PluginPageContext::requestFinished, observer, [this, observer, requestId](const QString& replyId, bool ok, const QVariantMap&, const QString& error) {
                if (replyId != requestId) return;
                model_.setStatus(ok ? tr("插件命令已完成。") : error); observer->deleteLater();
            });
            model_.setStatus(tr("正在执行插件命令…"));
        }
        return;
    }
    if (action == QStringLiteral("remove")) { remove(id); return; }
    if (action == QStringLiteral("uninstall-system")) {
        const auto record = records_.find(id);
        if (record != records_.end() && !record->second.manifest.drivers.isEmpty())
            runManagement(record->second.manifest.providerId(record->second.manifest.drivers.front().id), QStringLiteral("uninstall"));
        return;
    }
    if (action == QStringLiteral("restore-bundled")) { restoreBundled(id); return; }
    if (action == QStringLiteral("manage")) {
        const auto record = records_.find(id);
        if (record != records_.end() && !record->second.manifest.drivers.isEmpty())
            runManagement(record->second.manifest.providerId(record->second.manifest.drivers.front().id), QStringLiteral("repair"));
        return;
    }
    if (action == QStringLiteral("open")) {
        for (const auto& item : model_.state().value("pages").toList()) if (item.toMap().value("pluginId").toString() == id) {
            emit navigationRequested(item.toMap().value("id").toString()); return;
        }
        return;
    }
    if (action == QStringLiteral("restart")) { emit restartRequested(); return; }
    if (action == QStringLiteral("import")) {
        const DialogGuard guard(*this);
        const auto path = QFileDialog::getOpenFileName(window_, tr("导入插件"), {}, tr("插件包 (*.zip)"));
        if (path.isEmpty()) return;
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly) || file.size() > 64 * 1024 * 1024) { fail(tr("无法读取插件包或文件超过大小限制。")); return; }
        stage(file.readAll(), QDir::toNativeSeparators(QFileInfo(path).absoluteFilePath())); return;
    }
    if (action == QStringLiteral("install") || action == QStringLiteral("update") || action == QStringLiteral("repair-plugin")) {
        const bool repair = action == QStringLiteral("repair-plugin");
        const auto plugin = catalogPlugin(catalog_, id);
        const auto installed = records_.find(id);
        if (repair && installed == records_.end()) return;
        const auto version = catalogVersion(catalog_, id, repair ? installed->second.manifest.version : QString());
        if (!plugin.value(QStringLiteral("listed")).toBool() || version.isEmpty()) { fail(tr("没有兼容且允许安装的版本。")); return; }
        if (!validSha256(version.value(QStringLiteral("contentsSha256")).toString())) {
            fail(tr("此版本缺少可信文件校验信息，请刷新插件目录后重试。")); return;
        }
        const auto number = version.value(QStringLiteral("version")).toString();
        const auto reason = blockedReason(catalog_, id, number, version.value(QStringLiteral("sha256")).toString());
        if (!reason.isEmpty()) { fail(reason); return; }
        if (!confirm(repair ? tr("重新下载插件") : tr("下载插件"), tr("%1 v%2\n发布者：%3\n作者自述：%4\n审核：%5\n发布仓库：%6\n下载地址：%7\n\n下载完成后，你可以确认是否安装。")
            .arg(plugin.value(QStringLiteral("name")).toString(), number, plugin.value(QStringLiteral("publisher")).toString(),
                plugin.value(QStringLiteral("author")).toString(),
                version.value(QStringLiteral("reviewed")).toBool() ? tr("此版本审核通过") : tr("未审核，可由你选择安装"),
                plugin.value(QStringLiteral("repository")).toString(), version.value(QStringLiteral("url")).toString()), tr("下载"))) return;
        const auto hash = version.value(QStringLiteral("sha256")).toString();
        QStringList sources{version.value(QStringLiteral("url")).toString()};
        for (const auto mirror : version.value(QStringLiteral("mirrors")).toArray()) sources.append(mirror.toString());
        sources.removeDuplicates();
        downloadPackage(sources, id, hash, repair);
    }
}

void PluginService::pollActiveDriver() {
    auto* driver = activeDriver();
    if (shuttingDown_ || busy() || !driver || !driver->connected() || healthBusy_.load()) return;
    const auto id = QString::fromStdString(config_.activeDriverId);
    if (id == healthFailure_ || !available(id)) return;
    int interval = 750;
    const auto record = records_.find(preferenceId(id));
    if (record != records_.end()) for (const auto& provider : record->second.manifest.drivers)
        if (provider.id == id.section(QLatin1Char('/'), 1) && provider.heartbeatMs) interval = provider.heartbeatMs;
    const auto now = QDateTime::currentMSecsSinceEpoch();
    if (now - lastHeartbeat_ < interval) return;
    lastHeartbeat_ = now;
    const bool notify = std::exchange(deviceChangePending_, false);
    const auto generation = healthGeneration_;
    if (healthWorker_.joinable()) healthWorker_.join();
    healthBusy_.store(true);
    healthWorker_ = std::thread([this, driver, id, notify, generation] {
        const bool ok = (!notify || driver->control(XJ_CONTROL_DEVICE_CHANGE)) && driver->IsReady() && driver->control(XJ_CONTROL_HEARTBEAT);
        const auto message = ok ? QString() : driver->text(XJ_TEXT_ERROR);
        QMetaObject::invokeMethod(this, [this, driver, id, ok, message, generation] {
            if (!shuttingDown_ && generation == healthGeneration_ && !ok && id == QString::fromStdString(config_.activeDriverId) && driver == activeDriver()) {
                healthFailure_ = id;
                emit driverFailed(id, message.isEmpty() ? tr("当前驱动连接已失效。") : message);
            }
        }, Qt::QueuedConnection);
        healthBusy_.store(false);
    });
}
}
