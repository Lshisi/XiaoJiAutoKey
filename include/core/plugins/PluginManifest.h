#pragma once

#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <QVector>
#include <QMap>

namespace Plugins {
struct DriverContribution {
    QString id;
    QString name;
    QStringList deviceTokens;
    int heartbeatMs = 0;
    bool diagnostics = false;
    bool selfCheck = false;
};

struct PageContribution {
    QString id;
    QString title;
    QString file;
    QString icon;
    QString iconFile;
};

struct ManagementCommand {
    QString file;
    QStringList arguments;
    QString confirmation;
};

struct WindowContribution {
    QString id;
    QString title;
    QString file;
    int width = 320;
    int height = 240;
    bool transparent = false;
    bool frameless = false;
    bool topmost = false;
    bool focusable = true;
};

struct CommandContribution {
    QString id;
    QString title;
    QString target = QStringLiteral("background");
};

struct Manifest {
    QString id;
    QString name;
    QString version;
    QString author;
    QString description;
    QString minHostVersion;
    QString maxHostVersion;
    QString native;
    QString directory;
    QString contentsSha256; // Assigned by the host, never read from plugin.json.
    QStringList platforms;
    QStringList permissions;
    QStringList resources;
    QVector<DriverContribution> drivers;
    QVector<PageContribution> pages;
    QString backgroundFile;
    bool backgroundNative = false;
    QVector<WindowContribution> windows;
    QVector<CommandContribution> commands;
    QMap<QString, ManagementCommand> management;
    bool builtin = false;

    bool hasBackground() const { return !backgroundFile.isEmpty() || backgroundNative; }

    QString providerId(const QString& localId) const { return id + QLatin1Char('/') + localId; }
    QString pageId(const QString& localId) const { return QStringLiteral("plugin:") + providerId(localId); }
    QVariantMap presentation() const;
};

bool validPluginId(const QString& id);
bool validVersion(const QString& version);
int compareVersions(const QString& left, const QString& right);
QString containedFile(const QString& directory, const QString& relative, bool mustExist = true);
bool parseManifest(const QJsonObject& object, const QString& directory, Manifest* result, QString* error);
bool readManifest(const QString& path, Manifest* result, QString* error);
}
