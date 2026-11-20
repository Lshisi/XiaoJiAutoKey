#pragma once

#include "SoundSettings.h"
#include <QVariantMap>

namespace SoundSettingsData {
QVariantMap build(const SoundSettings& settings, SoundGroup group);
bool edit(QVariantMap& state, const QString& field, const QVariant& value);
SoundSettings settings(const QVariantMap& state);
QVariantMap selected(const QVariantMap& state);
void setSelectedPath(QVariantMap& state, const QString& path);
}
