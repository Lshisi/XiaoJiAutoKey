#pragma once

#include "core/plugins/PluginManifest.h"
#include <xiaoji/plugin/api.h>
#include <QObject>
#include <QByteArray>
#include <QElapsedTimer>
#include <memory>
#include <mutex>

namespace Plugins {
class NativeModule final : public QObject {
    Q_OBJECT
public:
    static std::shared_ptr<NativeModule> acquire(const Manifest& manifest, const QString& hostDirectory,
        const QString& dataDirectory, QString* error);
    static bool isLoaded(const QString& pluginId);
    ~NativeModule() override;
    XjResult create(const QString& provider, XjDriverApi* output);
    XjResult startBackground();
    void stopBackground();
    XjResult request(const QString& id, const QString& command, const QVariantMap& payload);

signals:
    void replied(const QString& id, bool ok, const QVariantMap& result, const QString& error);
    void published(const QString& topic, const QVariantMap& payload);

private:
    bool open(const Manifest& manifest, const QString& hostDirectory, const QString& dataDirectory, QString* error);
    bool allowEvent();
    void* library_ = nullptr;
    const XjPluginApi* api_ = nullptr;
    XjHostApi host_{};
    void* instance_ = nullptr;
    bool initialized_ = false;
    QByteArray id_, directory_, hostDirectory_, dataDirectory_;
    std::mutex eventMutex_;
    QElapsedTimer eventClock_;
    int eventCount_ = 0;
};
}
