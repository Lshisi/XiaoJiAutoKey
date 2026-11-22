#include "PluginPageHost.h"
#include "QmlIconProvider.h"
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQmlError>

PluginPageHost::PluginPageHost(QQuickItem* parent) : QQuickItem(parent) {}
PluginPageHost::~PluginPageHost() { clear(); }
void PluginPageHost::clear() {
    delete content_;
    content_ = nullptr;
    delete engine_;
    engine_ = nullptr;
}
void PluginPageHost::setSource(const QUrl& source) {
    if (source_ == source) return;
    source_ = source;
    emit sourceChanged();
    scheduleReload();
}
void PluginPageHost::setApi(QObject* api) {
    if (api_ == api) return;
    disconnect(destroyedConnection_);
    api_ = api;
    if (api_) destroyedConnection_ = connect(api_, &QObject::destroyed, this, [this] { api_.clear(); clear(); emit apiChanged(); });
    emit apiChanged();
    scheduleReload();
}
void PluginPageHost::componentComplete() { QQuickItem::componentComplete(); complete_ = true; scheduleReload(); }
void PluginPageHost::scheduleReload() {
    if (!complete_ || reloadPending_) return;
    reloadPending_ = true;
    QMetaObject::invokeMethod(this, [this] { reloadPending_ = false; reload(); }, Qt::QueuedConnection);
}
void PluginPageHost::geometryChange(const QRectF& now, const QRectF& before) {
    QQuickItem::geometryChange(now, before);
    if (content_) content_->setSize(now.size());
}
void PluginPageHost::reload() {
    if (!complete_) return;
    clear();
    error_.clear();
    if (!api_ || source_.isEmpty()) { emit errorChanged(); return; }
    if (!source_.isLocalFile() && source_.scheme() != QStringLiteral("qrc")) {
        error_ = tr("插件页面必须来自已安装的本地文件。"); emit errorChanged(); return;
    }
    engine_ = new QQmlEngine(this);
    engine_->addImageProvider(QStringLiteral("app-icons"), new QmlIconProvider);
    // A separate engine does not inherit the host window's private page models.
    // This is an API boundary, not a security sandbox for arbitrary QML code.
    engine_->rootContext()->setContextProperty(QStringLiteral("appView"), api_.data());
    QQmlComponent component(engine_, source_, QQmlComponent::PreferSynchronous);
    QObject* object = component.createWithInitialProperties({{QStringLiteral("pluginApi"), QVariant::fromValue(api_.data())}}, engine_->rootContext());
    content_ = qobject_cast<QQuickItem*>(object);
    if (!content_) {
        delete object;
        QStringList errors;
        for (const auto& error : component.errors()) errors.append(error.toString());
        error_ = errors.isEmpty() ? tr("插件页面根对象必须是可显示的 Item。") : errors.join(QLatin1Char('\n'));
        delete engine_; engine_ = nullptr;
    } else {
        content_->setParent(this);
        content_->setParentItem(this);
        content_->setSize(size());
    }
    emit errorChanged();
}
