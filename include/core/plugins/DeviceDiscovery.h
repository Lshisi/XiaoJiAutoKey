#pragma once
#include <QString>
#include <QStringList>
namespace Plugins {
QString detectSerialPort(const QStringList& identityTokens);
}
