#include "core/plugins/NativePluginModule.h"
#include "core/plugins/PluginIntegrity.h"
#include "DiagLog.h"
#include <QDir>
#include <QFileInfo>
#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <Windows.h>
#include <map>

namespace Plugins {
namespace {
struct ModuleCache {
    std::mutex mutex;
    std::mutex loadedMutex;
    std::map<QString, std::shared_ptr<NativeModule>> modules;
    QSet<QString> pluginIds;
    QSet<QString> directories;
};
ModuleCache& moduleCache() { static ModuleCache cache; return cache; }
QString boundedText(const char* text, size_t maximum) {
    if (!text) return {};
    size_t length = 0;
    while (length <= maximum && text[length]) ++length;
    return length > maximum ? QString() : QString::fromUtf8(text, static_cast<qsizetype>(length));
}
bool jsonObject(const char* json, uint32_t length, QVariantMap* result) {
    if (length > 65536 || (length && !json)) return false;
    if (!length) { *result = {}; return true; }
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(QByteArray(json, length), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) return false;
    *result = document.object().toVariantMap(); return true;
}
}

NativeModule::~NativeModule() {
    if (initialized_ && api_ && api_->shutdown) { try { api_->shutdown(instance_); } catch (...) {} }
    if (library_) FreeLibrary(static_cast<HMODULE>(library_));
}

std::shared_ptr<NativeModule> NativeModule::acquire(const Manifest& manifest, const QString& hostDirectory,
    const QString& dataDirectory, QString* error) {
    if (!QDir().mkpath(dataDirectory)) { if (error) *error = tr("无法创建插件数据目录。"); return {}; }
    // Keep the module and host callback context alive until process exit, including driver hook users.
    auto& cache = moduleCache();
    const auto key = containedFile(manifest.directory, manifest.native);
    std::lock_guard lock(cache.mutex);
    const auto found = cache.modules.find(key);
    if (found != cache.modules.end()) return found->second;
    auto module = std::make_shared<NativeModule>();
    if (auto* application = QCoreApplication::instance()) module->moveToThread(application->thread());
    if (!module->open(manifest, hostDirectory, dataDirectory, error)) return {};
    cache.modules.emplace(key, module);
    { std::lock_guard loadedLock(cache.loadedMutex);
        cache.pluginIds.insert(manifest.id);
        cache.directories.insert(QFileInfo(manifest.directory).canonicalFilePath().toCaseFolded());
    }
    return module;
}

bool NativeModule::isLoaded(const QString& pluginId) {
    auto& cache = moduleCache();
    std::lock_guard lock(cache.loadedMutex);
    return cache.pluginIds.contains(pluginId);
}

bool NativeModule::isLoadedDirectory(const QString& directory) {
    auto& cache = moduleCache();
    std::lock_guard lock(cache.loadedMutex);
    return cache.directories.contains(QFileInfo(directory).canonicalFilePath().toCaseFolded());
}

bool NativeModule::open(const Manifest& manifest, const QString& hostDirectory, const QString& dataDirectory, QString* error) {
    if (!verifyManifestContents(manifest, error)) return false;
    id_ = manifest.id.toUtf8();
    directory_ = manifest.directory.toUtf8();
    hostDirectory_ = hostDirectory.toUtf8();
    dataDirectory_ = dataDirectory.toUtf8();
    const auto fail = [&](const QString& message) { if (error) *error = message; return false; };
    const auto path = containedFile(manifest.directory, manifest.native);
    if (path.isEmpty()) return fail(tr("插件模块路径无效。"));
    library_ = LoadLibraryExW(reinterpret_cast<LPCWSTR>(path.utf16()), nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!library_) return fail(tr("无法加载插件模块（Windows 错误 %1）。").arg(GetLastError()));
    const auto entry = reinterpret_cast<XjGetPluginApi>(GetProcAddress(static_cast<HMODULE>(library_), XJ_PLUGIN_ENTRY));
    if (!entry) return fail(tr("插件模块未导出小叽插件入口。"));
    try {
        api_ = entry();
        if (!XJ_HAS_FIELD(api_, XjPluginApi, shutdown) || api_->abi_version != XJ_PLUGIN_ABI_VERSION || !api_->plugin_id || !api_->plugin_version
            || !api_->initialize || !api_->shutdown || (!manifest.drivers.isEmpty() && !api_->create_driver))
            return fail(tr("插件二进制接口不兼容。"));
        if (QString::fromUtf8(api_->plugin_id) != manifest.id || QString::fromUtf8(api_->plugin_version) != manifest.version)
            return fail(tr("插件模块身份或版本与清单不一致。"));
        host_ = {sizeof(XjHostApi), XJ_PLUGIN_ABI_VERSION, this,
            [](void* context, uint32_t, const char* text) {
                auto* module = static_cast<NativeModule*>(context);
                DiagLog::Write("plugin[" + module->id_.toStdString() + "] " + boundedText(text, 2000).toStdString());
            },
            [](void* context, const char* id, XjResult status, const char* json, uint32_t length) {
                auto* module = static_cast<NativeModule*>(context);
                const auto requestId = boundedText(id, 64);
                QVariantMap result;
                if (requestId.isEmpty() || !module->allowEvent()) return;
                const bool valid = jsonObject(json, length, &result);
                emit module->replied(requestId, valid && status == XJ_OK, result,
                    !valid ? tr("原生后台返回了无效的 JSON 对象。") : status == XJ_OK ? QString() : tr("原生后台处理请求失败（%1）。").arg(status));
            },
            [](void* context, const char* topic, const char* json, uint32_t length) {
                auto* module = static_cast<NativeModule*>(context);
                const auto name = boundedText(topic, 80);
                QVariantMap payload;
                if (!name.isEmpty() && module->allowEvent() && jsonObject(json, length, &payload)) emit module->published(name, payload);
            }};
        const XjPluginPaths paths{sizeof(XjPluginPaths), directory_.constData(), dataDirectory_.constData(), hostDirectory_.constData()};
        const auto status = api_->initialize(&host_, &paths, &instance_);
        initialized_ = status == XJ_OK || instance_ != nullptr;
        if (status != XJ_OK) return fail(tr("插件初始化失败。"));
        return true;
    } catch (...) { initialized_ = instance_ != nullptr; return fail(tr("插件在初始化时抛出异常。")); }
}

bool NativeModule::allowEvent() {
    std::lock_guard lock(eventMutex_);
    if (!eventClock_.isValid() || eventClock_.elapsed() >= 1000) { eventClock_.start(); eventCount_ = 0; }
    return ++eventCount_ <= 128;
}

XjResult NativeModule::create(const QString& provider, XjDriverApi* output) {
    if (!api_->create_driver) return XJ_UNSUPPORTED;
    const auto name = provider.toUtf8();
    try { return api_->create_driver(instance_, name.constData(), output); } catch (...) { return XJ_ERROR; }
}
XjResult NativeModule::startBackground() {
    if (!XJ_HAS_FIELD(api_, XjPluginApi, stop_background) || !api_->start_background || !api_->stop_background) return XJ_UNSUPPORTED;
    try { return api_->start_background(instance_); } catch (...) { return XJ_ERROR; }
}
void NativeModule::stopBackground() {
    if (XJ_HAS_FIELD(api_, XjPluginApi, stop_background) && api_->stop_background) { try { api_->stop_background(instance_); } catch (...) {} }
}
XjResult NativeModule::request(const QString& id, const QString& command, const QVariantMap& payload) {
    if (!XJ_HAS_FIELD(api_, XjPluginApi, handle_request) || !api_->handle_request) return XJ_UNSUPPORTED;
    const auto requestId = id.toUtf8(), name = command.toUtf8();
    const auto json = QJsonDocument::fromVariant(payload).toJson(QJsonDocument::Compact);
    try { return api_->handle_request(instance_, requestId.constData(), name.constData(), json.constData(), static_cast<uint32_t>(json.size())); }
    catch (...) { return XJ_ERROR; }
}
}
