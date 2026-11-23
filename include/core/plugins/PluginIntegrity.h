#pragma once

#include <QString>

namespace Plugins {
struct Manifest;
bool validSha256(const QString& value);
QString packageContentsHash(const QString& directory, QString* error);
bool verifyManifestContents(const Manifest& manifest, QString* error);
}
