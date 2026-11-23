#pragma once

#include "core/plugins/PluginManifest.h"
#include <QByteArray>

namespace Plugins {
struct StagedPackage {
    Manifest manifest;
    QString directory;
    QString sha256;
    QString contentsSha256;
};
bool stagePackage(const QByteArray& zip, const QString& cacheDirectory, StagedPackage* package, QString* error);
}
