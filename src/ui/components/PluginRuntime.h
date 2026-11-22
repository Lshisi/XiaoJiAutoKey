#pragma once

#include "core/plugins/PluginManifest.h"
#include <QObject>
#include <QTimer>
#include <QElapsedTimer>
#include <map>

class PluginPageContext;
class QQmlEngine;

namespace Plugins {
class PluginWindows;
class PluginRuntime final : public QObject {
    Q_OBJECT
public:
    PluginRuntime(Manifest manifest, PluginPageContext* api, const QVariantMap& windowState, QObject* parent = nullptr);
    ~PluginRuntime() override;
    QString version() const { return manifest_.version; }
    QString directory() const { return manifest_.directory; }
    QVariantMap presentation() const;
    void startBackground(const QString& reason = QStringLiteral("user"));
    void stopBackground(const QString& reason = QStringLiteral("user"));
    void shutdown(const QString& reason);
    void windowAction(const QString& operation, const QString& id, const QVariantMap& options = {});
    void nativeStarted(bool ok, const QString& error);
    void nativeStopped();
    void nativeReply(const QString& id, bool ok, const QVariantMap& result, const QString& error);
    void nativeEvent(const QString& topic, const QVariantMap& payload);

signals:
    void changed();
    void windowStateSaved(const QVariantMap& state);
    void backgroundRequested(bool running);
    void backgroundStarted();
    void backgroundFailed(const QString& error);
    void nativeStartRequested();
    void nativeStopRequested();
    void nativeRequestIssued(const QString& id, const QString& command, const QVariantMap& payload);
    void nativeRequestEnded(const QString& id);

private:
    struct Pending { QString target; qint64 deadline; };
    void startQml();
    void update();
    void failStart(const QString& error);
    void request(const QString& id, const QString& command, const QVariantMap& payload);
    void finish(const QString& id, const QString& target, bool ok, const QVariantMap& result, const QString& error);
    void cancelRequests(const QString& reason, const QString& target = {});
    Manifest manifest_;
    PluginPageContext* api_;
    QQmlEngine* engine_;
    PluginWindows* windows_;
    QObject* background_ = nullptr;
    QTimer requestTimer_;
    QElapsedTimer requestClock_;
    std::map<QString, Pending> pending_;
    QString status_ = QStringLiteral("stopped");
    QString reason_;
    QString error_;
    bool active_ = true;
};
}
