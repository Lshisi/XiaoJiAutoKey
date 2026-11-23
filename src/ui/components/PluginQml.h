#pragma once

#include "core/plugins/PluginManifest.h"
#include "PluginPageContext.h"
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQmlError>
#include <QUrl>

namespace Plugins {
inline QUrl qmlSource(const Manifest& manifest, const QString& file) {
    if (manifest.directory.startsWith(QStringLiteral(":/")))
        return QUrl(QStringLiteral("qrc") + manifest.directory + QLatin1Char('/') + file);
    const auto path = containedFile(manifest.directory, file);
    return path.isEmpty() ? QUrl() : QUrl::fromLocalFile(path);
}

inline QObject* createQml(QQmlEngine* engine, const QUrl& source, const QVariantMap& properties, QString* error) {
    if (source.isEmpty()) { *error = QObject::tr("插件 QML 入口缺失。"); return nullptr; }
    if (auto* context = properties.value(QStringLiteral("pluginApi")).value<PluginPageContext*>(); context && !context->verifyFiles(error)) return nullptr;
    QQmlComponent component(engine, source, QQmlComponent::PreferSynchronous);
    auto* object = component.createWithInitialProperties(properties);
    if (!object) {
        QStringList messages;
        for (const auto& item : component.errors()) messages.append(item.toString());
        *error = messages.join(QLatin1Char('\n')).left(4000);
    }
    return object;
}
}
