#pragma once

#include "core/plugins/PluginManifest.h"
#include <QObject>
#include <QThread>
#include <memory>
#include <atomic>
#include <map>

namespace Plugins {
class PluginNativeBackend final : public QObject {
    Q_OBJECT
public:
    PluginNativeBackend(Manifest manifest, QString hostDirectory, QObject* parent);
    ~PluginNativeBackend() override;
    void start();
    void stop();
    void request(const QString& id, const QString& command, const QVariantMap& payload);
    void cancelRequest(const QString& id);

signals:
    void initialized(bool ok, const QString& error);
    void started(bool ok, const QString& error);
    void stopped();
    void replied(const QString& id, bool ok, const QVariantMap& result, const QString& error);
    void published(const QString& topic, const QVariantMap& payload);

private:
    bool load(QString* error);
    void reportStarted(quint64 generation, bool ok, const QString& error);
    void reportStopped(quint64 generation);
    struct State;
    std::shared_ptr<State> state_;
    Manifest manifest_;
    QString hostDirectory_;
    QThread thread_;
    QObject* worker_;
    std::map<QString, std::shared_ptr<std::atomic<bool>>> requests_;
    quint64 lifecycleGeneration_ = 0;
};
}
