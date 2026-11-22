#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QString>

namespace Plugins {
bool decodeCatalog(const QByteArray& envelope, const QByteArray& publicKey, qint64 minimumRevision,
    QJsonObject* catalog, QString* error);
QString blockedReason(const QJsonObject& catalog, const QString& id, const QString& version, const QString& packageHash);
QJsonObject catalogVersion(const QJsonObject& catalog, const QString& id, const QString& version = {});
QJsonObject catalogPlugin(const QJsonObject& catalog, const QString& id);
}
