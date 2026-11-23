#include "PluginPageContext.h"
#include "ui/styles/QmlTheme.h"
#include <QJsonDocument>
#include <QRegularExpression>
#include <QUuid>

namespace {
bool validMessageName(const QString& name) {
    static const QRegularExpression pattern(QStringLiteral("^[a-z0-9][a-z0-9_.-]{0,79}$"));
    return pattern.match(name).hasMatch();
}
bool validPayload(const QVariantMap& payload) {
    return QJsonDocument::fromVariant(payload).toJson(QJsonDocument::Compact).size() <= 65536;
}
}

PluginPageContext::PluginPageContext(QString id, QStringList permissions, QObject* parent)
    : QObject(parent), id_(std::move(id)), permissions_(std::move(permissions)) {}

QVariantMap PluginPageContext::theme() const { return Fluent::qmlTheme(); }

bool PluginPageContext::verifyFiles(QString* error) const {
    if (!active_) { if (error) *error = tr("插件已停用，无法加载文件。"); return false; }
    return !contentsVerifier_ || contentsVerifier_(error);
}

void PluginPageContext::setRuntimeState(const QVariantMap& state) {
    if ((!permissions_.contains(QStringLiteral("engine.read")) && !permissions_.contains(QStringLiteral("engine.control"))) || state_ == state) return;
    state_ = state;
    emit stateChanged();
}

void PluginPageContext::setSettings(const QVariantMap& settings) {
    if (settings_ == settings) return;
    settings_ = settings;
    emit settingsChanged();
}

void PluginPageContext::setRuntime(const QVariantMap& runtime) {
    if (runtime_ == runtime) return;
    runtime_ = runtime;
    emit runtimeChanged();
}

void PluginPageContext::deactivate() { active_ = false; state_.clear(); emit stateChanged(); }

bool PluginPageContext::saveSetting(const QString& key, const QVariant& value) {
    if (!active_ || !permissions_.contains(QStringLiteral("settings.own")) || key.isEmpty() || key.size() > 64) return false;
    auto next = settings_;
    next.insert(key, value);
    if (next.size() > 128 || QJsonDocument::fromVariant(next).toJson(QJsonDocument::Compact).size() > 65536) return false;
    setSettings(next);
    emit settingEdited(key, value);
    return true;
}

void PluginPageContext::navigate(const QString& pageId) { if (active_ && pageId.size() <= 180) emit navigationRequested(pageId); }
void PluginPageContext::requestEngine(bool macro, bool running) {
    if (active_ && permissions_.contains(QStringLiteral("engine.control"))) emit engineRequested(macro, running);
}
void PluginPageContext::log(const QString& message) {
    if (active_ && permissions_.contains(QStringLiteral("log.write"))) emit logRequested(message.left(2000));
}

void PluginPageContext::showMessage(const QString& title, const QString& message) {
    if (active_ && permissions_.contains(QStringLiteral("ui.page")) && !title.isEmpty() && title.size() <= 80 && message.size() <= 2000)
        emit messageRequested(title, message);
}

void PluginPageContext::requestConfirmation(const QString& requestId, const QString& title, const QString& message) {
    if (!active_ || !permissions_.contains(QStringLiteral("ui.page")) || requestId.isEmpty() || requestId.size() > 64
        || title.isEmpty() || title.size() > 80 || message.size() > 2000) return;
    emit confirmationRequested(requestId, title, message);
}

QString PluginPageContext::request(const QString& command, const QVariantMap& payload) {
    if (!active_ || !permissions_.contains(QStringLiteral("messaging.own")) || !validMessageName(command) || !validPayload(payload) || !allowMessage()) return {};
    const auto id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    emit requestIssued(id, command, payload);
    return id;
}

void PluginPageContext::reply(const QString& requestId, bool ok, const QVariantMap& result, const QString& error) {
    if (active_ && permissions_.contains(QStringLiteral("messaging.own")) && requestId.size() <= 64 && validPayload(result) && allowMessage())
        emit replyIssued(requestId, ok, result, error.left(2000));
}

void PluginPageContext::publish(const QString& topic, const QVariantMap& payload) {
    if (active_ && permissions_.contains(QStringLiteral("messaging.own")) && validMessageName(topic) && validPayload(payload) && allowMessage())
        emit eventPublished(topic, payload);
}

bool PluginPageContext::allowMessage() {
    if (!messageClock_.isValid() || messageClock_.elapsed() >= 1000) { messageClock_.start(); messageCount_ = 0; }
    return ++messageCount_ <= 128;
}

bool PluginPageContext::setBackgroundRunning(bool running) {
    if (!active_ || !permissions_.contains(QStringLiteral("runtime.background"))) return false;
    emit backgroundRequested(running);
    return true;
}

bool PluginPageContext::showWindow(const QString& id) {
    if (!active_ || !permissions_.contains(QStringLiteral("ui.window")) || !validMessageName(id)) return false;
    emit windowRequested(QStringLiteral("show"), id, {}); return true;
}
bool PluginPageContext::hideWindow(const QString& id) {
    if (!active_ || !permissions_.contains(QStringLiteral("ui.window")) || !validMessageName(id)) return false;
    emit windowRequested(QStringLiteral("hide"), id, {}); return true;
}
bool PluginPageContext::closeWindow(const QString& id) {
    if (!active_ || !permissions_.contains(QStringLiteral("ui.window")) || !validMessageName(id)) return false;
    emit windowRequested(QStringLiteral("close"), id, {}); return true;
}
bool PluginPageContext::configureWindow(const QString& id, const QVariantMap& options) {
    if (!active_ || !permissions_.contains(QStringLiteral("ui.window")) || !validMessageName(id) || !validPayload(options)) return false;
    emit windowRequested(QStringLiteral("configure"), id, options); return true;
}
bool PluginPageContext::startWindowMove(const QString& id) {
    if (!active_ || !permissions_.contains(QStringLiteral("ui.window")) || !validMessageName(id)) return false;
    emit windowRequested(QStringLiteral("move"), id, {}); return true;
}
