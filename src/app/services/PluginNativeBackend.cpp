#include "services/PluginNativeBackend.h"
#include "core/plugins/NativePluginModule.h"
#include <QDir>
#include <utility>

namespace Plugins {
struct PluginNativeBackend::State {
    std::shared_ptr<NativeModule> module;
    bool started = false;
};

PluginNativeBackend::PluginNativeBackend(Manifest manifest, QString hostDirectory, QObject* parent)
    : QObject(parent), state_(std::make_shared<State>()), manifest_(std::move(manifest)),
      hostDirectory_(std::move(hostDirectory)), worker_(new QObject) {
    worker_->moveToThread(&thread_);
    connect(&thread_, &QThread::finished, worker_, &QObject::deleteLater);
    thread_.start();
}

PluginNativeBackend::~PluginNativeBackend() {
    for (auto& [id, canceled] : requests_) canceled->store(true);
    const auto state = state_;
    QMetaObject::invokeMethod(worker_, [state] {
        if (state->started && state->module) state->module->stopBackground();
        state->started = false;
    }, Qt::BlockingQueuedConnection);
    thread_.quit(); thread_.wait();
}

bool PluginNativeBackend::load(QString* error) {
    if (state_->module) return true;
    state_->module = NativeModule::acquire(manifest_, hostDirectory_,
        QDir(hostDirectory_).filePath(QStringLiteral("plugin-data/") + manifest_.id), error);
    if (!state_->module) { emit initialized(false, error ? *error : QString()); return false; }
    connect(state_->module.get(), &NativeModule::replied, this, &PluginNativeBackend::replied, Qt::QueuedConnection);
    connect(state_->module.get(), &NativeModule::published, this, &PluginNativeBackend::published, Qt::QueuedConnection);
    emit initialized(true, {});
    return true;
}

void PluginNativeBackend::start() {
    const auto generation = ++lifecycleGeneration_;
    QMetaObject::invokeMethod(worker_, [this, generation] {
        QString error;
        if (!load(&error)) { reportStarted(generation, false, error); return; }
        if (state_->started) { reportStarted(generation, true, {}); return; }
        const auto status = state_->module->startBackground();
        state_->started = status == XJ_OK;
        if (!state_->started) state_->module->stopBackground();
        reportStarted(generation, state_->started, state_->started ? QString() : tr("原生后台启动失败或未实现后台接口（%1）。").arg(status));
    });
}
void PluginNativeBackend::stop() {
    const auto generation = ++lifecycleGeneration_;
    for (auto& [id, canceled] : requests_) canceled->store(true);
    requests_.clear();
    QMetaObject::invokeMethod(worker_, [this, generation] {
        if (state_->started && state_->module) state_->module->stopBackground();
        state_->started = false;
        reportStopped(generation);
    });
}
void PluginNativeBackend::reportStarted(quint64 generation, bool ok, const QString& error) {
    QMetaObject::invokeMethod(this, [this, generation, ok, error] {
        if (generation == lifecycleGeneration_) emit started(ok, error);
    }, Qt::QueuedConnection);
}
void PluginNativeBackend::reportStopped(quint64 generation) {
    QMetaObject::invokeMethod(this, [this, generation] {
        if (generation == lifecycleGeneration_) emit stopped();
    }, Qt::QueuedConnection);
}
void PluginNativeBackend::request(const QString& id, const QString& command, const QVariantMap& payload) {
    if (requests_.size() >= 32) { emit replied(id, false, {}, tr("原生后台待处理请求过多。")); return; }
    const auto canceled = std::make_shared<std::atomic<bool>>(false);
    requests_.emplace(id, canceled);
    QMetaObject::invokeMethod(worker_, [this, id, command, payload, canceled] {
        if (canceled->load()) return;
        QString error;
        if (!load(&error)) { emit replied(id, false, {}, error); return; }
        if (canceled->load()) return;
        const auto status = state_->module->request(id, command, payload);
        if (status != XJ_OK) emit replied(id, false, {}, tr("原生后台拒绝此请求或未实现命令接口（%1）。").arg(status));
    });
}
void PluginNativeBackend::cancelRequest(const QString& id) {
    const auto found = requests_.find(id);
    if (found == requests_.end()) return;
    found->second->store(true);
    requests_.erase(found);
}
}
