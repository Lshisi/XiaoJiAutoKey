#include "core/plugins/PluginManifest.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSet>
#include <QVersionNumber>

namespace Plugins {
namespace {
bool fail(QString* error, const QString& message) { if (error) *error = message; return false; }
bool localId(const QString& value) {
    static const QRegularExpression pattern(QStringLiteral("^[a-z0-9][a-z0-9_-]{0,47}$"));
    return pattern.match(value).hasMatch();
}
bool strings(const QJsonValue& value, QStringList* output, int maximum, bool required = false) {
    if (value.isUndefined() && !required) return true;
    if (!value.isArray() || value.toArray().size() > maximum) return false;
    for (const auto item : value.toArray()) {
        if (!item.isString() || item.toString().isEmpty() || item.toString().size() > 256) return false;
        output->append(item.toString());
    }
    return !required || !output->isEmpty();
}
bool within(const QString& root, const QString& path) {
    const QString prefix = QDir::cleanPath(root) + QLatin1Char('/');
    return QDir::fromNativeSeparators(path).startsWith(prefix, Qt::CaseInsensitive);
}
}

bool validPluginId(const QString& id) {
    static const QRegularExpression pattern(QStringLiteral("^[a-z0-9][a-z0-9.-]{1,94}[a-z0-9]$"));
    return pattern.match(id).hasMatch() && !id.contains(QStringLiteral(".."));
}

bool validVersion(const QString& version) {
    static const QRegularExpression pattern(QStringLiteral("^(0|[1-9][0-9]{0,8})\\.(0|[1-9][0-9]{0,8})\\.(0|[1-9][0-9]{0,8})$"));
    return pattern.match(version).hasMatch();
}

int compareVersions(const QString& left, const QString& right) {
    return QVersionNumber::compare(QVersionNumber::fromString(left), QVersionNumber::fromString(right));
}

QString containedFile(const QString& directory, const QString& relative, bool mustExist) {
    QString name = QDir::fromNativeSeparators(relative);
    if (name.isEmpty() || name.contains(QLatin1Char(':')) || QDir::isAbsolutePath(name)
        || name.startsWith(QLatin1Char('/')) || name.contains(QChar::Null)) return {};
    const auto segments = name.split(QLatin1Char('/'), Qt::KeepEmptyParts);
    for (const auto& segment : segments)
        if (segment.isEmpty() || segment == QStringLiteral("..") || segment == QStringLiteral(".")
            || segment.endsWith(QLatin1Char('.')) || segment.endsWith(QLatin1Char(' '))) return {};
    const QString root = QFileInfo(directory).canonicalFilePath();
    if (root.isEmpty()) return {};
    const QString target = QDir(root).absoluteFilePath(name);
    if (!within(root, target)) return {};
    const QFileInfo info(target);
    if (info.isSymLink()) return {};
    if (mustExist) {
        const QString canonical = info.canonicalFilePath();
        if (!info.isFile() || canonical.isEmpty() || !within(root, canonical)) return {};
        return canonical;
    }
    QString existing = target;
    while (!QFileInfo::exists(existing)) {
        const QString parent = QFileInfo(existing).absolutePath();
        if (parent == existing) return {};
        existing = parent;
    }
    const QString canonicalParent = QFileInfo(existing).canonicalFilePath();
    if (canonicalParent.compare(root, Qt::CaseInsensitive) != 0 && !within(root, canonicalParent)) return {};
    return target;
}

bool parseManifest(const QJsonObject& object, const QString& directory, Manifest* result, QString* error) {
    if (!result) return fail(error, QStringLiteral("插件清单输出无效。"));
    if (object.value(QStringLiteral("schemaVersion")).toInt(-1) != 1
        || object.value(QStringLiteral("apiVersion")).toInt(-1) != 1)
        return fail(error, QStringLiteral("此插件需要不同的清单或 API 版本。"));
    Manifest manifest;
    manifest.directory = QFileInfo(directory).canonicalFilePath();
    manifest.id = object.value(QStringLiteral("id")).toString();
    manifest.name = object.value(QStringLiteral("name")).toString().trimmed();
    manifest.version = object.value(QStringLiteral("version")).toString();
    manifest.author = object.value(QStringLiteral("author")).toString().trimmed();
    manifest.description = object.value(QStringLiteral("description")).toString().trimmed();
    manifest.minHostVersion = object.value(QStringLiteral("minHostVersion")).toString();
    manifest.maxHostVersion = object.value(QStringLiteral("maxHostVersion")).toString();
    manifest.native = object.value(QStringLiteral("native")).toString();
    if (!validPluginId(manifest.id) || !validVersion(manifest.version)
        || manifest.name.isEmpty() || manifest.name.size() > 80 || manifest.author.isEmpty() || manifest.author.size() > 120
        || manifest.description.isEmpty() || manifest.description.size() > 2000 || !validVersion(manifest.minHostVersion))
        return fail(error, QStringLiteral("插件标识、名称或版本格式无效。"));
    if (!manifest.maxHostVersion.isEmpty() && (!validVersion(manifest.maxHostVersion) || compareVersions(manifest.maxHostVersion, manifest.minHostVersion) < 0))
        return fail(error, QStringLiteral("插件最高宿主版本格式无效。"));
    if (compareVersions(QString::fromLatin1(APP_VERSION), manifest.minHostVersion) < 0
        || (!manifest.maxHostVersion.isEmpty() && compareVersions(QString::fromLatin1(APP_VERSION), manifest.maxHostVersion) > 0))
        return fail(error, QStringLiteral("插件与当前小叽版本不兼容。"));
    if (!strings(object.value(QStringLiteral("platforms")), &manifest.platforms, 8, true)
        || !manifest.platforms.contains(QStringLiteral("windows-x64")))
        return fail(error, QStringLiteral("此插件不支持 Windows x64。"));
    if (!strings(object.value(QStringLiteral("permissions")), &manifest.permissions, 32))
        return fail(error, QStringLiteral("插件能力声明无效。"));
    static const QSet<QString> supported{QStringLiteral("driver.provide"), QStringLiteral("device.identify"),
        QStringLiteral("device.serial"), QStringLiteral("ui.page"), QStringLiteral("engine.read"),
        QStringLiteral("engine.control"), QStringLiteral("settings.own"), QStringLiteral("log.write"), QStringLiteral("system.driver.manage"),
        QStringLiteral("runtime.background"), QStringLiteral("ui.window"), QStringLiteral("messaging.own")};
    for (const auto& permission : manifest.permissions)
        if (!supported.contains(permission)) return fail(error, QStringLiteral("框架不支持插件申请的能力：%1").arg(permission));
    if (!strings(object.value(QStringLiteral("resources")), &manifest.resources, 32))
        return fail(error, QStringLiteral("插件资源清单无效。"));
    for (const auto& resource : manifest.resources)
        if (containedFile(directory, resource, false).isEmpty() || !QFileInfo::exists(QDir(directory).filePath(resource)))
            return fail(error, QStringLiteral("插件声明的资源缺失或路径无效。"));
    if (!manifest.native.isEmpty() && (!manifest.native.endsWith(QStringLiteral(".dll"), Qt::CaseInsensitive)
        || containedFile(directory, manifest.native).isEmpty()))
        return fail(error, QStringLiteral("插件原生模块缺失或路径无效。"));
    QSet<QString> ids;
    const auto drivers = object.value(QStringLiteral("drivers"));
    if (!drivers.isUndefined() && (!drivers.isArray() || drivers.toArray().size() > 16))
        return fail(error, QStringLiteral("驱动提供方清单无效。"));
    for (const auto value : drivers.toArray()) {
        const auto row = value.toObject();
        DriverContribution driver;
        for (const auto* name : {"diagnostics", "selfCheck"})
            if (row.contains(QLatin1String(name)) && !row.value(QLatin1String(name)).isBool())
                return fail(error, QStringLiteral("驱动诊断能力声明无效。"));
        driver.diagnostics = row.value(QStringLiteral("diagnostics")).toBool();
        driver.selfCheck = row.value(QStringLiteral("selfCheck")).toBool();
        driver.id = row.value(QStringLiteral("id")).toString();
        driver.name = row.value(QStringLiteral("name")).toString().trimmed();
        driver.heartbeatMs = row.value(QStringLiteral("heartbeatMs")).toInt(0);
        if (!value.isObject() || !localId(driver.id) || ids.contains(driver.id) || driver.name.isEmpty()
            || driver.name.size() > 80 || !strings(row.value(QStringLiteral("deviceTokens")), &driver.deviceTokens, 24)
            || (driver.heartbeatMs && (driver.heartbeatMs < 250 || driver.heartbeatMs > 60000)))
            return fail(error, QStringLiteral("驱动提供方标识、设备身份或心跳配置无效。"));
        for (auto& token : driver.deviceTokens) {
            token = token.trimmed().toLower();
            if (token.size() < 5) return fail(error, QStringLiteral("驱动设备身份条件过宽。"));
        }
        if (!driver.deviceTokens.isEmpty() && !manifest.permissions.contains(QStringLiteral("device.identify")))
            return fail(error, QStringLiteral("设备身份声明需要 device.identify 能力。"));
        ids.insert(driver.id);
        manifest.drivers.push_back(driver);
    }
    if (!manifest.drivers.isEmpty() && (manifest.native.isEmpty() || !manifest.permissions.contains(QStringLiteral("driver.provide"))))
        return fail(error, QStringLiteral("驱动插件缺少原生入口或 driver.provide 声明。"));
    ids.clear();
    const auto pages = object.value(QStringLiteral("pages"));
    if (!pages.isUndefined() && (!pages.isArray() || pages.toArray().size() > 8))
        return fail(error, QStringLiteral("页面清单无效。"));
    for (const auto value : pages.toArray()) {
        const auto row = value.toObject();
        PageContribution page;
        page.id = row.value(QStringLiteral("id")).toString();
        page.title = row.value(QStringLiteral("title")).toString().trimmed();
        page.file = row.value(QStringLiteral("file")).toString();
        page.icon = row.value(QStringLiteral("icon")).toString(QStringLiteral("puzzle"));
        static const QSet<QString> icons{QStringLiteral("puzzle"), QStringLiteral("keyboard"), QStringLiteral("repeat"),
            QStringLiteral("monitor"), QStringLiteral("settings"), QStringLiteral("info"), QStringLiteral("bar-chart"), QStringLiteral("music")};
        if (!icons.contains(page.icon)) page.icon = QStringLiteral("puzzle");
        if (!value.isObject() || !localId(page.id) || ids.contains(page.id) || page.title.isEmpty()
            || page.title.size() > 80 || !page.file.endsWith(QStringLiteral(".qml"), Qt::CaseInsensitive)
            || containedFile(directory, page.file).isEmpty())
            return fail(error, QStringLiteral("插件页面标识、名称或文件无效。"));
        ids.insert(page.id);
        manifest.pages.push_back(page);
    }
    if (!manifest.pages.isEmpty() && !manifest.permissions.contains(QStringLiteral("ui.page")))
        return fail(error, QStringLiteral("页面插件缺少 ui.page 声明。"));
    const auto background = object.value(QStringLiteral("background"));
    if (!background.isUndefined()) {
        if (!background.isObject()) return fail(error, QStringLiteral("插件后台声明无效。"));
        const auto row = background.toObject();
        const auto file = row.value(QStringLiteral("file"));
        const auto native = row.value(QStringLiteral("native"));
        if ((!file.isUndefined() && (!file.isString() || file.toString().isEmpty())) || (!native.isUndefined() && !native.isBool()))
            return fail(error, QStringLiteral("插件后台入口格式无效。"));
        manifest.backgroundFile = file.toString();
        manifest.backgroundNative = native.toBool();
        if (!manifest.hasBackground() || !manifest.permissions.contains(QStringLiteral("runtime.background"))
            || (manifest.backgroundNative && manifest.native.isEmpty())
            || (!manifest.backgroundFile.isEmpty() && (!manifest.backgroundFile.endsWith(QStringLiteral(".qml"), Qt::CaseInsensitive)
                || containedFile(directory, manifest.backgroundFile).isEmpty())))
            return fail(error, QStringLiteral("插件后台缺少有效入口或 runtime.background 声明。"));
    }
    ids.clear();
    const auto windows = object.value(QStringLiteral("windows"));
    if (!windows.isUndefined() && (!windows.isArray() || windows.toArray().size() > 8))
        return fail(error, QStringLiteral("插件窗口清单无效。"));
    for (const auto value : windows.toArray()) {
        const auto row = value.toObject();
        WindowContribution window;
        window.id = row.value(QStringLiteral("id")).toString();
        window.title = row.value(QStringLiteral("title")).toString().trimmed();
        window.file = row.value(QStringLiteral("file")).toString();
        for (const auto& key : {QStringLiteral("transparent"), QStringLiteral("frameless"), QStringLiteral("topmost")})
            if (row.contains(key) && !row.value(key).isBool()) return fail(error, QStringLiteral("插件窗口选项格式无效。"));
        window.width = row.value(QStringLiteral("width")).toInt(320);
        window.height = row.value(QStringLiteral("height")).toInt(240);
        if (!value.isObject() || !localId(window.id) || ids.contains(window.id) || window.title.isEmpty() || window.title.size() > 80
            || !window.file.endsWith(QStringLiteral(".qml"), Qt::CaseInsensitive) || containedFile(directory, window.file).isEmpty()
            || window.width < 32 || window.width > 2048 || window.height < 32 || window.height > 2048
            || (row.contains(QStringLiteral("width")) && (!row.value(QStringLiteral("width")).isDouble() || row.value(QStringLiteral("width")).toDouble() != window.width))
            || (row.contains(QStringLiteral("height")) && (!row.value(QStringLiteral("height")).isDouble() || row.value(QStringLiteral("height")).toDouble() != window.height)))
            return fail(error, QStringLiteral("插件窗口标识、文件或尺寸无效。"));
        window.transparent = row.value(QStringLiteral("transparent")).toBool();
        window.frameless = row.value(QStringLiteral("frameless")).toBool();
        window.topmost = row.value(QStringLiteral("topmost")).toBool();
        ids.insert(window.id);
        manifest.windows.push_back(window);
    }
    if (!manifest.windows.isEmpty() && !manifest.permissions.contains(QStringLiteral("ui.window")))
        return fail(error, QStringLiteral("插件窗口需要 ui.window 能力声明。"));
    ids.clear();
    const auto commands = object.value(QStringLiteral("commands"));
    if (!commands.isUndefined() && (!commands.isArray() || commands.toArray().size() > 32))
        return fail(error, QStringLiteral("插件命令清单无效。"));
    for (const auto value : commands.toArray()) {
        const auto row = value.toObject();
        CommandContribution command;
        command.id = row.value(QStringLiteral("id")).toString();
        command.title = row.value(QStringLiteral("title")).toString().trimmed();
        command.target = row.value(QStringLiteral("target")).toString(QStringLiteral("background"));
        if (!value.isObject() || !localId(command.id) || ids.contains(command.id) || command.title.isEmpty() || command.title.size() > 80
            || (row.contains(QStringLiteral("target")) && !row.value(QStringLiteral("target")).isString())
            || (command.target != QStringLiteral("background") && command.target != QStringLiteral("native"))
            || (command.target == QStringLiteral("background") && manifest.backgroundFile.isEmpty())
            || (command.target == QStringLiteral("native") && manifest.native.isEmpty()))
            return fail(error, QStringLiteral("插件命令标识、名称或目标无效。"));
        ids.insert(command.id);
        manifest.commands.push_back(command);
    }
    if (!manifest.commands.isEmpty() && !manifest.permissions.contains(QStringLiteral("messaging.own")))
        return fail(error, QStringLiteral("插件命令需要 messaging.own 能力声明。"));
    if (manifest.drivers.isEmpty() && manifest.pages.isEmpty() && !manifest.hasBackground() && manifest.windows.isEmpty())
        return fail(error, QStringLiteral("插件没有声明驱动、页面、后台或窗口。"));
    const auto managementValue = object.value(QStringLiteral("management"));
    if (!managementValue.isUndefined() && !managementValue.isObject()) return fail(error, QStringLiteral("插件管理工具声明无效。"));
    const auto management = managementValue.toObject();
    for (auto iterator = management.begin(); iterator != management.end(); ++iterator) {
        if (iterator.key() != QStringLiteral("install") && iterator.key() != QStringLiteral("repair") && iterator.key() != QStringLiteral("uninstall"))
            return fail(error, QStringLiteral("插件声明了不支持的管理操作。"));
        const auto row = iterator.value().toObject();
        ManagementCommand command;
        command.file = row.value(QStringLiteral("file")).toString();
        command.confirmation = row.value(QStringLiteral("confirmation")).toString().trimmed();
        if (!iterator.value().isObject() || !command.file.endsWith(QStringLiteral(".exe"), Qt::CaseInsensitive)
            || containedFile(directory, command.file).isEmpty() || command.confirmation.isEmpty() || command.confirmation.size() > 1500
            || !strings(row.value(QStringLiteral("arguments")), &command.arguments, 24))
            return fail(error, QStringLiteral("插件管理工具、参数或操作说明无效。"));
        manifest.management.insert(iterator.key(), command);
    }
    if (!manifest.management.isEmpty() && !manifest.permissions.contains(QStringLiteral("system.driver.manage")))
        return fail(error, QStringLiteral("管理工具需要 system.driver.manage 能力声明。"));
    *result = std::move(manifest);
    if (error) error->clear();
    return true;
}

bool readManifest(const QString& path, Manifest* result, QString* error) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly) || file.size() > 128 * 1024)
        return fail(error, QStringLiteral("无法读取插件清单，或清单超过大小限制。"));
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
        return fail(error, QStringLiteral("插件清单不是有效的 JSON 对象。"));
    return parseManifest(document.object(), QFileInfo(path).absolutePath(), result, error);
}

QVariantMap Manifest::presentation() const {
    return {{QStringLiteral("id"), id}, {QStringLiteral("name"), name}, {QStringLiteral("version"), version},
        {QStringLiteral("author"), author}, {QStringLiteral("description"), description},
        {QStringLiteral("minHostVersion"), minHostVersion}, {QStringLiteral("maxHostVersion"), maxHostVersion},
        {QStringLiteral("hasDriver"), !drivers.isEmpty()}, {QStringLiteral("hasPage"), !pages.isEmpty()},
        {QStringLiteral("hasBackground"), hasBackground()}, {QStringLiteral("hasWindow"), !windows.isEmpty()},
        {QStringLiteral("builtin"), builtin}, {QStringLiteral("permissions"), permissions}, {QStringLiteral("hasManagement"), !management.isEmpty()}};
}
}
