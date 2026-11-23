#include "core/plugins/PluginIntegrity.h"
#include "core/plugins/PluginManifest.h"
#include <QByteArrayView>
#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QSet>
#include <QVector>
#include <QtEndian>
#include <algorithm>

namespace Plugins {
bool validSha256(const QString& value) {
    return value.size() == 64 && std::all_of(value.cbegin(), value.cend(), [](QChar character) {
        const auto code = character.unicode();
        return (code >= '0' && code <= '9') || (code >= 'a' && code <= 'f') || (code >= 'A' && code <= 'F');
    });
}

QString packageContentsHash(const QString& directory, QString* error) {
    const auto fail = [&](const QString& message) { if (error) *error = message; return QString(); };
    const QFileInfo root(directory);
    if (!root.isDir() || root.isSymLink() || root.isJunction()) return fail(QStringLiteral("插件版本目录无效或包含链接。"));
    struct Entry { QByteArray path; quint64 size; QByteArray hash; };
    QVector<Entry> entries;
    QSet<QString> names;
    quint64 total = 0;
    QDirIterator iterator(directory, QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    while (iterator.hasNext()) {
        iterator.next();
        const auto info = iterator.fileInfo();
        if (info.isSymLink() || info.isJunction()) return fail(QStringLiteral("插件版本目录包含链接，请重新安装。"));
        if (info.isDir()) continue;
        if (!info.isFile()) return fail(QStringLiteral("插件版本目录包含非普通文件。"));
        const auto relative = QDir(directory).relativeFilePath(info.absoluteFilePath());
        if (relative.compare(QStringLiteral(".installation.json"), Qt::CaseInsensitive) == 0) continue;
        if (containedFile(directory, relative).isEmpty()) return fail(QStringLiteral("插件文件超出版本目录。"));
        const auto folded = relative.toCaseFolded();
        if (names.contains(folded)) return fail(QStringLiteral("插件目录包含大小写冲突的文件。"));
        names.insert(folded);
        if (info.size() < 0 || info.size() > 64 * 1024 * 1024 || entries.size() >= 4096)
            return fail(QStringLiteral("插件文件数量或大小超过限制。"));
        total += static_cast<quint64>(info.size());
        if (total > 256 * 1024 * 1024) return fail(QStringLiteral("插件版本目录超过大小限制。"));
        QFile file(info.absoluteFilePath());
        QCryptographicHash digest(QCryptographicHash::Sha256);
        if (!file.open(QIODevice::ReadOnly) || !digest.addData(&file) || file.size() != info.size())
            return fail(QStringLiteral("无法完整读取插件文件，请重新安装。"));
        entries.append({relative.toUtf8(), static_cast<quint64>(info.size()), digest.result()});
    }
    if (!names.contains(QStringLiteral("plugin.json"))) return fail(QStringLiteral("插件清单缺失，请重新安装。"));
    std::sort(entries.begin(), entries.end(), [](const Entry& left, const Entry& right) {
        return std::lexicographical_compare(left.path.cbegin(), left.path.cend(), right.path.cbegin(), right.path.cend(),
            [](char a, char b) { return static_cast<unsigned char>(a) < static_cast<unsigned char>(b); });
    });
    QCryptographicHash digest(QCryptographicHash::Sha256);
    digest.addData(QByteArrayLiteral("XiaoJiPluginContents\0v1\0"));
    for (const auto& entry : entries) {
        char length[4], size[8];
        qToBigEndian<quint32>(static_cast<quint32>(entry.path.size()), length);
        qToBigEndian<quint64>(entry.size, size);
        digest.addData(QByteArrayView(length, 4));
        digest.addData(entry.path);
        digest.addData(QByteArrayView(size, 8));
        digest.addData(entry.hash);
    }
    if (error) error->clear();
    return QString::fromLatin1(digest.result().toHex());
}

bool verifyManifestContents(const Manifest& manifest, QString* error) {
    if (manifest.contentsSha256.isEmpty()) return true;
    if (!validSha256(manifest.contentsSha256)) {
        if (error) *error = QStringLiteral("插件文件校验信息无效，请重新安装。");
        return false;
    }
    const auto actual = packageContentsHash(manifest.directory, error);
    if (actual.isEmpty()) return false;
    if (actual.compare(manifest.contentsSha256, Qt::CaseInsensitive) != 0) {
        if (error) *error = QStringLiteral("插件文件已变化或缺失，已停止加载，请重新安装该版本。");
        return false;
    }
    return true;
}
}
