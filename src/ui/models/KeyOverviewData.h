#pragma once

#include "AppTypes.h"
#include <QVariantMap>

namespace KeyOverviewData {
QVariantMap key(const KeyDescriptor& descriptor, const QString& text);
KeyDescriptor descriptor(const QVariantMap& key);
QVariantMap build(const QVariantMap& source, bool macroMode, QString& selectedId);
QVariantMap action(const QVariantMap& overview, const QString& targetId);
}
