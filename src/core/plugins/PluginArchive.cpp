#include "core/plugins/PluginArchive.h"
#include "core/plugins/PluginIntegrity.h"
#include "miniz.h"
#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QSaveFile>
#include <QSet>
#include <QUuid>

namespace Plugins {
namespace {
bool reservedName(const QString& name) {
    const auto base = name.section(QLatin1Char('.'), 0, 0).toUpper();
    return base == QStringLiteral("CON") || base == QStringLiteral("PRN") || base == QStringLiteral("AUX")
        || base == QStringLiteral("NUL") || (base.size() == 4 && (base.startsWith(QStringLiteral("COM"))
        || base.startsWith(QStringLiteral("LPT"))) && base[3] >= QLatin1Char('1') && base[3] <= QLatin1Char('9'));
}
}

bool stagePackage(const QByteArray& data, const QString& cacheDirectory, StagedPackage* package, QString* error) {
    QString staging;
    const auto fail = [&](const QString& message) {
        if (error) *error = message;
        if (!staging.isEmpty()) QDir(staging).removeRecursively();
        return false;
    };
    if (!package || data.isEmpty() || data.size() > 64 * 1024 * 1024) return fail(QStringLiteral("插件包为空或超过 64 MB。"));
    mz_zip_archive zip{};
    if (!mz_zip_reader_init_mem(&zip, data.constData(), static_cast<size_t>(data.size()), 0)) return fail(QStringLiteral("插件包不是有效的 zip。"));
    struct Close { mz_zip_archive* value; ~Close() { mz_zip_reader_end(value); } } close{&zip};
    const auto count = mz_zip_reader_get_num_files(&zip);
    if (!count || count > 4096) return fail(QStringLiteral("插件包文件数量无效。"));
    struct Entry { mz_uint index; QString name; };
    QVector<Entry> entries;
    QSet<QString> names;
    quint64 total = 0;
    for (mz_uint i = 0; i < count; ++i) {
        mz_zip_archive_file_stat stat{};
        if (!mz_zip_reader_file_stat(&zip, i, &stat)) return fail(QStringLiteral("无法读取插件包文件信息。"));
        QString name = QString::fromUtf8(stat.m_filename);
        name.replace(QLatin1Char('\\'), QLatin1Char('/'));
        if (name.endsWith(QLatin1Char('/'))) name.chop(1);
        const auto segments = name.split(QLatin1Char('/'), Qt::KeepEmptyParts);
        if (name.isEmpty() || name.contains(QLatin1Char(':')) || name.contains(QChar::Null)
            || name.startsWith(QLatin1Char('/')) || ((stat.m_external_attr >> 16) & 0170000) == 0120000)
            return fail(QStringLiteral("插件包包含非法路径或链接。"));
        for (const auto& segment : segments)
            if (segment.isEmpty() || segment == QStringLiteral("..") || segment == QStringLiteral(".")
                || segment.endsWith(QLatin1Char('.')) || segment.endsWith(QLatin1Char(' ')) || reservedName(segment))
                return fail(QStringLiteral("插件包包含非法文件名。"));
        if (stat.m_is_directory) continue;
        const auto folded = name.toCaseFolded();
        if (folded == QStringLiteral(".installation.json")) return fail(QStringLiteral("插件包不能包含宿主安装记录。"));
        if (names.contains(folded)) return fail(QStringLiteral("插件包包含重复路径。"));
        names.insert(folded);
        total += stat.m_uncomp_size;
        if (total > 256 * 1024 * 1024 || stat.m_uncomp_size > 64 * 1024 * 1024) return fail(QStringLiteral("插件包解压内容超过大小限制。"));
        entries.push_back({i, name});
    }
    if (!names.contains(QStringLiteral("plugin.json"))) return fail(QStringLiteral("插件包根目录缺少 plugin.json。"));
    staging = QDir(cacheDirectory).absoluteFilePath(QStringLiteral("staging/") + QUuid::createUuid().toString(QUuid::WithoutBraces));
    if (!QDir().mkpath(staging)) return fail(QStringLiteral("无法创建插件暂存目录。"));
    for (const auto& entry : entries) {
        const auto path = containedFile(staging, entry.name, false);
        if (path.isEmpty() || !QDir().mkpath(QFileInfo(path).absolutePath())) return fail(QStringLiteral("无法创建插件文件目录。"));
        QSaveFile file(path);
        if (!file.open(QIODevice::WriteOnly)) return fail(QStringLiteral("无法写入插件文件。"));
        struct Sink { QSaveFile* file; bool ok = true; } sink{&file};
        const auto extracted = mz_zip_reader_extract_to_callback(&zip, entry.index,
            [](void* context, mz_uint64, const void* bytes, size_t amount) -> size_t {
                auto& state = *static_cast<Sink*>(context);
                const auto written = state.file->write(static_cast<const char*>(bytes), static_cast<qint64>(amount));
                state.ok = written == static_cast<qint64>(amount);
                return state.ok ? amount : 0;
            }, &sink, 0);
        if (!extracted || !sink.ok || !file.commit()) return fail(QStringLiteral("插件包解压失败。"));
    }
    Manifest manifest;
    QString message;
    if (!readManifest(QDir(staging).filePath(QStringLiteral("plugin.json")), &manifest, &message)) return fail(message);
    const auto contentsHash = packageContentsHash(staging, &message);
    if (contentsHash.isEmpty()) return fail(message);
    manifest.contentsSha256 = contentsHash;
    package->manifest = std::move(manifest);
    package->directory = staging;
    package->sha256 = QString::fromLatin1(QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex());
    package->contentsSha256 = contentsHash;
    if (error) error->clear();
    return true;
}
}
