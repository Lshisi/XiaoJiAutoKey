#include "core/plugins/PluginCatalog.h"
#include "core/plugins/PluginManifest.h"
#include "core/plugins/PluginIntegrity.h"

#include <QCryptographicHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSet>
#include <QRegularExpression>
#include <QUrl>
#include <Windows.h>
#include <bcrypt.h>
#include <cstring>

namespace Plugins {
namespace {
bool fail(QString* error, const QString& message) { if (error) *error = message; return false; }
bool verify(const QByteArray& payload, const QByteArray& signature, const QByteArray& publicKey) {
    if (publicKey.size() != 65 || static_cast<unsigned char>(publicKey[0]) != 4 || signature.size() != 64) return false;
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    BCRYPT_KEY_HANDLE key = nullptr;
    if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_ECDSA_P256_ALGORITHM, nullptr, 0) < 0) return false;
    QByteArray blob(sizeof(BCRYPT_ECCKEY_BLOB) + 64, '\0');
    BCRYPT_ECCKEY_BLOB header{BCRYPT_ECDSA_PUBLIC_P256_MAGIC, 32};
    std::memcpy(blob.data(), &header, sizeof(header));
    std::memcpy(blob.data() + sizeof(header), publicKey.constData() + 1, 64);
    const auto imported = BCryptImportKeyPair(algorithm, nullptr, BCRYPT_ECCPUBLIC_BLOB, &key,
        reinterpret_cast<PUCHAR>(blob.data()), static_cast<ULONG>(blob.size()), 0);
    bool valid = false;
    if (imported >= 0) {
        auto digest = QCryptographicHash::hash(payload, QCryptographicHash::Sha256);
        valid = BCryptVerifySignature(key, nullptr, reinterpret_cast<PUCHAR>(digest.data()), static_cast<ULONG>(digest.size()),
            reinterpret_cast<PUCHAR>(const_cast<char*>(signature.constData())), static_cast<ULONG>(signature.size()), 0) >= 0;
        BCryptDestroyKey(key);
    }
    BCryptCloseAlgorithmProvider(algorithm, 0);
    return valid;
}
bool safeDownload(const QString& address) {
    const QUrl url(address);
    return url.isValid() && url.scheme() == QStringLiteral("https") && !url.host().isEmpty()
        && url.userInfo().isEmpty() && url.fragment().isEmpty() && (url.port(-1) == -1 || url.port() == 443);
}
bool validPublisher(const QJsonObject& plugin) {
    const auto address = plugin.value(QStringLiteral("repository")).toString();
    const QUrl url(address);
    if (!safeDownload(address) || !url.query().isEmpty()
        || (url.host() != QStringLiteral("github.com") && url.host() != QStringLiteral("gitee.com"))) return false;
    const auto parts = url.path().split(QLatin1Char('/'), Qt::SkipEmptyParts);
    static const QRegularExpression name(QStringLiteral("^[A-Za-z0-9_.-]+$"));
    if (parts.size() != 2) return false;
    for (const auto& part : parts)
        if (part == QStringLiteral(".") || part == QStringLiteral("..") || !name.match(part).hasMatch()) return false;
    const auto expected = url.host().section(QLatin1Char('.'), 0, 0) + QLatin1Char(':') + parts[0].toLower();
    return plugin.value(QStringLiteral("publisher")).toString() == expected;
}
}

bool decodeCatalog(const QByteArray& envelope, const QByteArray& publicKey, qint64 minimumRevision,
    QJsonObject* catalog, QString* error) {
    if (!catalog || envelope.size() > 8 * 1024 * 1024) return fail(error, QStringLiteral("插件目录超过大小限制。"));
    QJsonParseError parsed;
    const auto document = QJsonDocument::fromJson(envelope, &parsed);
    if (parsed.error != QJsonParseError::NoError || !document.isObject()) return fail(error, QStringLiteral("插件目录格式无效。"));
    const auto wrapper = document.object();
    if (wrapper.value(QStringLiteral("format")).toInt() != 1
        || wrapper.value(QStringLiteral("algorithm")).toString() != QStringLiteral("ecdsa-p256-sha256"))
        return fail(error, QStringLiteral("插件目录签名格式不受支持。"));
    const auto payloadResult = QByteArray::fromBase64Encoding(wrapper.value(QStringLiteral("payload")).toString().toLatin1(), QByteArray::AbortOnBase64DecodingErrors);
    const auto signatureResult = QByteArray::fromBase64Encoding(wrapper.value(QStringLiteral("signature")).toString().toLatin1(), QByteArray::AbortOnBase64DecodingErrors);
    if (!payloadResult || !signatureResult || payloadResult.decoded.size() > 4 * 1024 * 1024
        || !verify(payloadResult.decoded, signatureResult.decoded, publicKey))
        return fail(error, QStringLiteral("插件目录签名校验失败，已保留本地有效目录。"));
    const auto content = QJsonDocument::fromJson(payloadResult.decoded, &parsed);
    if (parsed.error != QJsonParseError::NoError || !content.isObject()) return fail(error, QStringLiteral("目录内容不是有效的 JSON 对象。"));
    const auto root = content.object();
    const auto revision = root.value(QStringLiteral("revision")).toVariant().toLongLong();
    if (root.value(QStringLiteral("format")).toInt() != 1 || revision < minimumRevision || revision <= 0
        || !root.value(QStringLiteral("plugins")).isArray() || root.value(QStringLiteral("plugins")).toArray().size() > 2048
        || !root.value(QStringLiteral("blocked")).isArray() || root.value(QStringLiteral("blocked")).toArray().size() > 4096)
        return fail(error, QStringLiteral("目录格式或修订号无效。"));
    QSet<QString> ids;
    for (const auto value : root.value(QStringLiteral("plugins")).toArray()) {
        const auto plugin = value.toObject();
        const auto id = plugin.value(QStringLiteral("id")).toString();
        if (!value.isObject() || !validPluginId(id) || ids.contains(id) || plugin.value(QStringLiteral("name")).toString().isEmpty()
            || plugin.value(QStringLiteral("name")).toString().size() > 80 || plugin.value(QStringLiteral("author")).toString().isEmpty()
            || plugin.value(QStringLiteral("author")).toString().size() > 120 || plugin.value(QStringLiteral("description")).toString().size() > 2000
            || !plugin.value(QStringLiteral("official")).isBool() || !plugin.value(QStringLiteral("listed")).isBool() || !validPublisher(plugin)
            || !plugin.value(QStringLiteral("versions")).isArray() || plugin.value(QStringLiteral("versions")).toArray().size() > 128)
            return fail(error, QStringLiteral("目录中存在无效或重复的插件。"));
        ids.insert(id);
        QSet<QString> versions;
        for (const auto versionValue : plugin.value(QStringLiteral("versions")).toArray()) {
            const auto version = versionValue.toObject();
            const auto number = version.value(QStringLiteral("version")).toString();
            const auto hash = version.value(QStringLiteral("sha256")).toString();
            const auto maximum = version.value(QStringLiteral("maxHostVersion")).toString();
            const auto contents = version.value(QStringLiteral("contentsSha256"));
            if (!versionValue.isObject() || !validVersion(number) || versions.contains(number) || !validSha256(hash)
                || (!contents.isUndefined() && (!contents.isString() || !validSha256(contents.toString()))) || !safeDownload(version.value(QStringLiteral("url")).toString())
                || !validVersion(version.value(QStringLiteral("minHostVersion")).toString()) || (!maximum.isEmpty() && !validVersion(maximum)))
                return fail(error, QStringLiteral("目录中存在无效的插件版本或下载来源。"));
            const auto mirrors = version.value(QStringLiteral("mirrors"));
            if (!mirrors.isUndefined() && (!mirrors.isArray() || mirrors.toArray().size() > 4))
                return fail(error, QStringLiteral("目录中的插件镜像来源无效。"));
            for (const auto mirror : mirrors.toArray())
                if (!mirror.isString() || !safeDownload(mirror.toString()))
                    return fail(error, QStringLiteral("插件镜像地址必须为有效的 HTTPS 下载来源。"));
            versions.insert(number);
        }
    }
    for (const auto value : root.value(QStringLiteral("blocked")).toArray()) {
        const auto rule = value.toObject();
        const auto number = rule.value(QStringLiteral("version")).toString();
        const auto hash = rule.value(QStringLiteral("sha256")).toString();
        if (!value.isObject() || !validPluginId(rule.value(QStringLiteral("id")).toString())
            || (number != QStringLiteral("*") && !validVersion(number))
            || (!hash.isEmpty() && (hash.size() != 64 || QByteArray::fromHex(hash.toLatin1()).size() != 32)))
            return fail(error, QStringLiteral("目录禁用策略格式无效。"));
    }
    *catalog = root;
    if (error) error->clear();
    return true;
}

QString blockedReason(const QJsonObject& catalog, const QString& id, const QString& version, const QString& packageHash) {
    for (const auto value : catalog.value(QStringLiteral("blocked")).toArray()) {
        const auto rule = value.toObject();
        if (rule.value(QStringLiteral("id")).toString() != id) continue;
        const auto affected = rule.value(QStringLiteral("version")).toString();
        if (affected != QStringLiteral("*") && affected != version) continue;
        const auto hash = rule.value(QStringLiteral("sha256")).toString();
        if (!hash.isEmpty() && hash.compare(packageHash, Qt::CaseInsensitive) != 0) continue;
        const auto reason = rule.value(QStringLiteral("reason")).toString().trimmed();
        return reason.isEmpty() ? QStringLiteral("此插件版本已被官方禁用。") : reason;
    }
    return {};
}

QJsonObject catalogPlugin(const QJsonObject& catalog, const QString& id) {
    for (const auto value : catalog.value(QStringLiteral("plugins")).toArray())
        if (value.toObject().value(QStringLiteral("id")).toString() == id) return value.toObject();
    return {};
}

QJsonObject catalogVersion(const QJsonObject& catalog, const QString& id, const QString& requested) {
    QJsonObject best;
    for (const auto value : catalogPlugin(catalog, id).value(QStringLiteral("versions")).toArray()) {
        const auto version = value.toObject();
        const auto number = version.value(QStringLiteral("version")).toString();
        if (!requested.isEmpty()) { if (number == requested) return version; continue; }
        if (compareVersions(QString::fromLatin1(APP_VERSION), version.value(QStringLiteral("minHostVersion")).toString()) < 0) continue;
        const auto maximum = version.value(QStringLiteral("maxHostVersion")).toString();
        if (!maximum.isEmpty() && compareVersions(QString::fromLatin1(APP_VERSION), maximum) > 0) continue;
        if (!blockedReason(catalog, id, number, version.value(QStringLiteral("sha256")).toString()).isEmpty()) continue;
        if (best.isEmpty() || compareVersions(number, best.value(QStringLiteral("version")).toString()) > 0) best = version;
    }
    return best;
}
}
