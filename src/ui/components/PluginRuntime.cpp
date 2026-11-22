#include "PluginRuntime.h"
#include "PluginPageContext.h"
#include "PluginQml.h"
#include "PluginWindows.h"
#include "QmlIconProvider.h"
#include <QJsonDocument>
#include <QQmlContext>
#include <QRegularExpression>
#include <algorithm>
#include <utility>
#include <QQuickItem>
#include <QWindow>

namespace Plugins {
PluginRuntime::PluginRuntime(Manifest manifest, PluginPageContext* api, const QVariantMap& windowState, QObject* parent)
    : QObject(parent), manifest_(std::move(manifest)), api_(api), engine_(new QQmlEngine(this)),
      windows_(new PluginWindows(manifest_, api, engine_, windowState, this)) {
    engine_->addImageProvider(QStringLiteral("app-icons"), new QmlIconProvider);
    QQmlEngine::setObjectOwnership(api_, QQmlEngine::CppOwnership);
    engine_->rootContext()->setContextProperty(QStringLiteral("appView"), api_);
    connect(windows_, &PluginWindows::changed, this, &PluginRuntime::update);
    connect(windows_, &PluginWindows::stateSaved, this, &PluginRuntime::windowStateSaved);
    connect(api_, &PluginPageContext::requestIssued, this, &PluginRuntime::request, Qt::QueuedConnection);
    connect(api_, &PluginPageContext::replyIssued, this, [this](const QString& id, bool ok, const QVariantMap& result, const QString& error) {
        finish(id, QStringLiteral("background"), ok, result, error);
    }, Qt::QueuedConnection);
    connect(api_, &PluginPageContext::eventPublished, this, [this](const QString& topic, const QVariantMap& payload) {
        if (active_ && status_ != QStringLiteral("stopping") && (!manifest_.hasBackground() || status_ == QStringLiteral("running")))
            emit api_->eventReceived(topic, payload);
    }, Qt::QueuedConnection);
    connect(api_, &PluginPageContext::backgroundRequested, this, &PluginRuntime::backgroundRequested, Qt::QueuedConnection);
    connect(api_, &PluginPageContext::windowRequested, this, [this](const QString& operation, const QString& id, const QVariantMap& options) {
        // startSystemMove needs the current pointer gesture; other operations may destroy QML objects.
        if (operation == QStringLiteral("move")) windowAction(operation, id, options);
        else QTimer::singleShot(0, this, [this, operation, id, options] { windowAction(operation, id, options); });
    });
    requestTimer_.setInterval(250);
    requestClock_.start();
    connect(&requestTimer_, &QTimer::timeout, this, [this] {
        const auto now = requestClock_.elapsed();
        QStringList expired;
        for (const auto& [id, request] : pending_) if (request.deadline <= now) expired.append(id);
        for (const auto& id : expired) {
            const auto found = pending_.find(id);
            if (found != pending_.end()) finish(id, found->second.target, false, {}, tr("插件请求超时。"));
        }
    });
    update();
}

PluginRuntime::~PluginRuntime() {
    shutdown(QStringLiteral("exit"));
    delete windows_;
    delete engine_;
}

QVariantMap PluginRuntime::presentation() const {
    QVariantList commands;
    for (const auto& command : manifest_.commands)
        commands.append(QVariantMap{{"id", command.id}, {"title", command.title}, {"target", command.target}});
    return {{"status", status_}, {"backgroundRunning", status_ == QStringLiteral("running")},
        {"error", error_}, {"windows", windows_->presentation()}, {"commands", commands}};
}

void PluginRuntime::update() {
    api_->setRuntime(presentation());
    emit changed();
}

void PluginRuntime::startBackground(const QString& reason) {
    if (!active_ || !manifest_.hasBackground() || status_ == QStringLiteral("running")
        || status_ == QStringLiteral("starting") || status_ == QStringLiteral("stopping")) return;
    reason_ = reason;
    error_.clear();
    status_ = QStringLiteral("starting");
    update();
    if (manifest_.backgroundNative) emit nativeStartRequested();
    else startQml();
}

void PluginRuntime::startQml() {
    if (!manifest_.backgroundFile.isEmpty()) {
        QString error;
        background_ = createQml(engine_, qmlSource(manifest_, manifest_.backgroundFile), {{"pluginApi", QVariant::fromValue(api_)}}, &error);
        if (!background_) { failStart(error.isEmpty() ? tr("无法创建插件后台。"): error); return; }
        if (qobject_cast<QQuickItem*>(background_) || qobject_cast<QWindow*>(background_)) {
            failStart(tr("插件后台根对象应为非可视 QtObject；窗口请使用 windows 声明。")); return;
        }
        background_->setParent(engine_);
        QQmlEngine::setObjectOwnership(background_, QQmlEngine::CppOwnership);
    }
    emit api_->lifecycleEvent(QStringLiteral("starting"), reason_);
    status_ = QStringLiteral("running");
    update();
    emit api_->lifecycleEvent(QStringLiteral("started"), reason_);
    emit backgroundStarted();
}

void PluginRuntime::failStart(const QString& error) {
    windows_->closeAll();
    delete background_; background_ = nullptr;
    if (manifest_.backgroundNative) emit nativeStopRequested();
    status_ = QStringLiteral("error");
    error_ = error.left(4000);
    update();
    emit backgroundFailed(error_);
}

void PluginRuntime::nativeStarted(bool ok, const QString& error) {
    if (!active_ || status_ != QStringLiteral("starting")) return;
    if (!ok) { failStart(error); return; }
    startQml();
}

void PluginRuntime::stopBackground(const QString& reason) {
    if (status_ == QStringLiteral("stopping")) return;
    const bool wasActive = status_ == QStringLiteral("running") || status_ == QStringLiteral("starting");
    status_ = QStringLiteral("stopping");
    update();
    emit api_->lifecycleEvent(QStringLiteral("stopping"), reason);
    cancelRequests(tr("插件后台已停止。"), QStringLiteral("background"));
    if (manifest_.backgroundNative) cancelRequests(tr("插件原生后台已停止。"), QStringLiteral("native"));
    windows_->closeAll();
    delete background_; background_ = nullptr;
    reason_ = reason;
    if (manifest_.backgroundNative && wasActive) emit nativeStopRequested();
    else nativeStopped();
}

void PluginRuntime::nativeStopped() {
    if (status_ != QStringLiteral("stopping")) return;
    status_ = QStringLiteral("stopped");
    update();
    emit api_->lifecycleEvent(QStringLiteral("stopped"), reason_);
}

void PluginRuntime::shutdown(const QString& reason) {
    if (!active_) return;
    active_ = false;
    stopBackground(reason);
    cancelRequests(tr("插件运行实例已关闭。"));
    // Page engines can still refer to the context until their scheduled cleanup.
    disconnect(api_, nullptr, this, nullptr);
}

void PluginRuntime::windowAction(const QString& operation, const QString& id, const QVariantMap& options) {
    if (!active_ || status_ == QStringLiteral("stopping")) return;
    if (!windows_->dispatch(operation, id, options)) error_ = windows_->error();
    else if (status_ != QStringLiteral("error")) error_.clear();
    update();
}

void PluginRuntime::request(const QString& id, const QString& command, const QVariantMap& payload) {
    if (!active_) return;
    const auto reject = [&](const QString& message) { emit api_->requestFinished(id, false, {}, message); };
    const auto found = std::find_if(manifest_.commands.cbegin(), manifest_.commands.cend(), [&](const auto& item) { return item.id == command; });
    if (found == manifest_.commands.cend()) { reject(tr("插件没有声明此命令。")); return; }
    if (pending_.size() >= 32) { reject(tr("插件待处理请求过多。")); return; }
    if (status_ == QStringLiteral("stopping") || ((found->target == QStringLiteral("background") || manifest_.backgroundNative)
        && status_ != QStringLiteral("running"))) { reject(tr("请先启动插件后台。")); return; }
    pending_.emplace(id, Pending{found->target, requestClock_.elapsed() + 10000});
    if (!requestTimer_.isActive()) requestTimer_.start();
    if (found->target == QStringLiteral("native")) emit nativeRequestIssued(id, command, payload);
    else emit api_->commandReceived(id, command, payload);
}

void PluginRuntime::finish(const QString& id, const QString& target, bool ok, const QVariantMap& result, const QString& error) {
    const auto found = pending_.find(id);
    if (found == pending_.end() || found->second.target != target) return;
    const bool native = found->second.target == QStringLiteral("native");
    pending_.erase(found);
    if (native) emit nativeRequestEnded(id);
    if (pending_.empty()) requestTimer_.stop();
    emit api_->requestFinished(id, ok, result, error.left(2000));
}

void PluginRuntime::cancelRequests(const QString& reason, const QString& target) {
    QStringList ids;
    for (const auto& [id, pending] : pending_) if (target.isEmpty() || pending.target == target) ids.append(id);
    for (const auto& id : ids) {
        const auto found = pending_.find(id);
        if (found != pending_.end()) finish(id, found->second.target, false, {}, reason);
    }
}

void PluginRuntime::nativeReply(const QString& id, bool ok, const QVariantMap& result, const QString& error) {
    if (active_) finish(id, QStringLiteral("native"), ok, result, error);
}

void PluginRuntime::nativeEvent(const QString& topic, const QVariantMap& payload) {
    static const QRegularExpression pattern(QStringLiteral("^[a-z0-9][a-z0-9_.-]{0,79}$"));
    if (active_ && manifest_.permissions.contains(QStringLiteral("messaging.own")) && status_ != QStringLiteral("stopping")
        && (!manifest_.backgroundNative || status_ == QStringLiteral("running")) && pattern.match(topic).hasMatch()
        && QJsonDocument::fromVariant(payload).toJson(QJsonDocument::Compact).size() <= 65536)
        emit api_->eventReceived(topic, payload);
}
}
