#pragma once

#include <QStringList>
#include <QVariantMap>

namespace GameProcessData {

QVariantMap create(const QStringList& processes, const QVariantList& windows = {});
void refresh(QVariantMap& data, const QVariantList& windows);
void add(QVariantMap& data, const QString& input);
void remove(QVariantMap& data, int index);

}  // namespace GameProcessData
