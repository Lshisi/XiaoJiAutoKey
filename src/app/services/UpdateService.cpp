#include "services/UpdateService.h"

#include "AppTypes.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QSaveFile>
#include <QThread>
#include <QTimer>

#include <Windows.h>
#include <winhttp.h>

#include "miniz.h"

#include <vector>

#pragma comment(lib, "winhttp.lib")

#ifndef WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_3
#define WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_3 0x2000
#endif

namespace {

constexpr char kReleasesApiUrl[] =
    "https://gitee.com/api/v5/repos/fourteen-fourteen/xiao-ji-auto-key/releases?per_page=30";

// Archives written by Windows PowerShell 5.1 may store entry names in the
// local code page instead of UTF-8; fall back when the UTF-8 decode is lossy.
QString decodeEntryName(const char* rawName) {
    const QString utf8 = QString::fromUtf8(rawName);
    if (!utf8.contains(QChar(0xFFFD))) {
        return utf8;
    }
    const QString local = QString::fromLocal8Bit(rawName);
    return local.contains(QChar(0xFFFD)) ? utf8 : local;
}

struct ExtractEntry {
    mz_uint index = 0;
    QString relativePath;
    bool isDirectory = false;
};

// Unpacks the release zip into destDir. Handles the top-level package folder
// (Compress-Archive includes the "XiaoJiAutoKey" directory itself) and the
// backslash entry separators written by Windows PowerShell archives.
QString extractZipToDir(const QString& zipPath, const QString& destDir) {
    QFile zipFile(zipPath);
    if (!zipFile.open(QIODevice::ReadOnly)) {
        return QString::fromWCharArray(L"\u65e0\u6cd5\u8bfb\u53d6\u4e0b\u8f7d\u7684\u66f4\u65b0\u5305");
    }
    const QByteArray archive = zipFile.readAll();
    zipFile.close();

    mz_zip_archive zip{};
    if (!mz_zip_reader_init_mem(&zip, archive.constData(), static_cast<size_t>(archive.size()), 0)) {
        return QString::fromWCharArray(L"\u66f4\u65b0\u5305\u683c\u5f0f\u65e0\u6548");
    }
    struct ReaderGuard {
        mz_zip_archive* zip;
        ~ReaderGuard() { mz_zip_reader_end(zip); }
    } readerGuard{&zip};

    const mz_uint fileCount = mz_zip_reader_get_num_files(&zip);
    if (fileCount == 0) {
        return QString::fromWCharArray(L"\u66f4\u65b0\u5305\u5185\u5bb9\u4e3a\u7a7a");
    }

    std::vector<ExtractEntry> entries;
    entries.reserve(fileCount);
    QString commonPrefix;
    bool hasCommonPrefix = true;
    for (mz_uint i = 0; i < fileCount; ++i) {
        mz_zip_archive_file_stat stat{};
        if (!mz_zip_reader_file_stat(&zip, i, &stat)) {
            return QString::fromWCharArray(L"\u66f4\u65b0\u5305\u683c\u5f0f\u65e0\u6548");
        }

        QString name = decodeEntryName(stat.m_filename);
        name.replace(QLatin1Char('\\'), QLatin1Char('/'));
        while (name.startsWith(QLatin1Char('/'))) {
            name.remove(0, 1);
        }
        if (name.isEmpty()) {
            continue;
        }

        const bool isDirectory = stat.m_is_directory != 0 || name.endsWith(QLatin1Char('/'));
        while (name.endsWith(QLatin1Char('/'))) {
            name.chop(1);
        }

        // Reject absolute paths and traversal outside the package.
        const QStringList segments = name.split(QLatin1Char('/'), Qt::KeepEmptyParts);
        if (name.contains(QLatin1Char(':')) || segments.contains(QStringLiteral("..")) ||
            segments.contains(QString())) {
            return QString::fromWCharArray(L"\u66f4\u65b0\u5305\u5185\u5bb9\u8def\u5f84\u5f02\u5e38");
        }

        ExtractEntry entry;
        entry.index = i;
        entry.relativePath = name;
        entry.isDirectory = isDirectory;
        entries.push_back(entry);

        if (name.contains(QLatin1Char('/'))) {
            const QString first = name.section(QLatin1Char('/'), 0, 0);
            if (commonPrefix.isEmpty()) {
                commonPrefix = first;
            } else if (commonPrefix != first) {
                hasCommonPrefix = false;
            }
        } else if (!isDirectory) {
            // A root-level file means the archive has no wrapper folder.
            // Directory entries at the root are the wrapper itself.
            hasCommonPrefix = false;
        }
    }

    if (hasCommonPrefix && !commonPrefix.isEmpty()) {
        const int prefixLength = commonPrefix.size() + 1;
        for (auto& entry : entries) {
            if (!entry.isDirectory) {
                entry.relativePath = entry.relativePath.mid(prefixLength);
            }
        }
    }

    for (const ExtractEntry& entry : entries) {
        if (entry.isDirectory || entry.relativePath.isEmpty()) {
            continue;
        }
        // The user configuration must never be overwritten by the package.
        if (entry.relativePath.compare(QStringLiteral("config.json"), Qt::CaseInsensitive) == 0) {
            continue;
        }
        if (entry.relativePath.endsWith(QStringLiteral(".old.exe"), Qt::CaseInsensitive)) {
            continue;
        }

        const QString targetPath = destDir + QLatin1Char('/') + entry.relativePath;
        if (!QDir().mkpath(QFileInfo(targetPath).absolutePath())) {
            return QString::fromWCharArray(L"\u65e0\u6cd5\u521b\u5efa\u89e3\u538b\u76ee\u5f55: ") + entry.relativePath;
        }

        QSaveFile output(targetPath);
        if (!output.open(QIODevice::WriteOnly)) {
            return QString::fromWCharArray(L"\u65e0\u6cd5\u5199\u5165\u66f4\u65b0\u6587\u4ef6: ") + entry.relativePath;
        }

        struct Sink {
            QSaveFile* file = nullptr;
            bool ok = true;
        } sink{&output};
        const mz_bool extracted = mz_zip_reader_extract_to_callback(&zip, entry.index,
            [](void* opaque, mz_uint64, const void* buffer, size_t count) -> size_t {
                auto* sink = static_cast<Sink*>(opaque);
                const qint64 written = sink->file->write(
                    static_cast<const char*>(buffer), static_cast<qint64>(count));
                if (written != static_cast<qint64>(count)) {
                    sink->ok = false;
                    return 0;
                }
                return count;
            },
            &sink, 0);
        if (!extracted || !sink.ok || !output.commit()) {
            return QString::fromWCharArray(L"\u89e3\u538b\u5931\u8d25: ") + entry.relativePath;
        }
    }

    return {};
}

}  // namespace

UpdateService::UpdateService(QObject* parent) : QObject(parent) {}

UpdateService::~UpdateService() {
    {
        std::lock_guard<std::mutex> lock(sessionMutex_);
        sessionClosed_ = true;
        if (session_) {
            // Closing the session aborts any in-flight WinHTTP call on the
            // worker thread so the join below cannot block for long.
            WinHttpCloseHandle(static_cast<HINTERNET>(session_));
            session_ = nullptr;
        }
    }
    if (worker_.joinable()) {
        worker_.join();
    }
}

void UpdateService::runWorker(std::function<void()> job) {
    if (workerBusy_.exchange(true)) {
        return;
    }
    if (worker_.joinable()) {
        worker_.join();
    }
    worker_ = std::thread([this, job = std::move(job)]() {
        job();
        workerBusy_.store(false);
    });
}

// Closes the WinHTTP session and unregisters it from the service, so the
// destructor can abort an in-flight request by closing the stored handle.
struct UpdateService::SessionGuard {
    UpdateService* self = nullptr;
    HINTERNET session = nullptr;

    ~SessionGuard() {
        if (session) {
            ::WinHttpCloseHandle(session);
        }
        std::lock_guard<std::mutex> lock(self->sessionMutex_);
        if (self->session_ == session) {
            self->session_ = nullptr;
        }
    }
};

bool UpdateService::fetchUrl(const QString& url, const std::function<bool(qint64, qint64)>& onProgress,
    QByteArray* body, unsigned long* statusCode, QString* error) {
    *statusCode = 0;
    body->clear();

    URL_COMPONENTS parts{};
    parts.dwStructSize = sizeof(parts);
    wchar_t host[256] = {};
    wchar_t path[2048] = {};
    parts.lpszHostName = host;
    parts.dwHostNameLength = 255;
    parts.lpszUrlPath = path;
    parts.dwUrlPathLength = 2047;
    const std::wstring wideUrl = url.toStdWString();
    if (!::WinHttpCrackUrl(wideUrl.c_str(), 0, 0, &parts) || parts.dwHostNameLength == 0) {
        if (error) *error = QString::fromWCharArray(L"\u66f4\u65b0\u5730\u5740\u65e0\u6548");
        return false;
    }

    const std::wstring userAgent = L"XiaoJiAutoKey/" + ToWide(APP_VERSION);
    HINTERNET session = ::WinHttpOpen(userAgent.c_str(), WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session) {
        if (error) *error = QString::fromWCharArray(L"\u7f51\u7edc\u8bf7\u6c42\u521d\u59cb\u5316\u5931\u8d25");
        return false;
    }
    {
        std::lock_guard<std::mutex> lock(sessionMutex_);
        if (sessionClosed_) {
            ::WinHttpCloseHandle(session);
            if (error) *error = QString::fromWCharArray(L"\u66f4\u65b0\u5df2\u53d6\u6d88");
            return false;
        }
        session_ = session;
    }
    SessionGuard sessionGuard{this, session};

    ::WinHttpSetTimeouts(session, 5000, 5000, 5000, 60000);

    DWORD secureProtocols = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2 | WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_3;
    if (!::WinHttpSetOption(session, WINHTTP_OPTION_SECURE_PROTOCOLS,
            &secureProtocols, sizeof(secureProtocols))) {
        secureProtocols = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2;
        ::WinHttpSetOption(session, WINHTTP_OPTION_SECURE_PROTOCOLS,
            &secureProtocols, sizeof(secureProtocols));
    }

    HINTERNET connection = ::WinHttpConnect(session, host, parts.nPort, 0);
    if (!connection) {
        if (error) *error = QString::fromWCharArray(L"\u65e0\u6cd5\u8fde\u63a5\u66f4\u65b0\u670d\u52a1\u5668");
        return false;
    }
    struct HandleGuard {
        HINTERNET handle;
        ~HandleGuard() {
            if (handle) ::WinHttpCloseHandle(handle);
        }
    } connectionGuard{connection}, requestGuard{nullptr};

    const DWORD requestFlags = (parts.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET request = ::WinHttpOpenRequest(connection, L"GET", path, nullptr,
        WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, requestFlags);
    if (!request) {
        if (error) *error = QString::fromWCharArray(L"\u65e0\u6cd5\u521b\u5efa\u66f4\u65b0\u8bf7\u6c42");
        return false;
    }
    requestGuard.handle = request;

    if (!::WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
            WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
        !::WinHttpReceiveResponse(request, nullptr)) {
        if (error) {
            *error = QString::fromWCharArray(L"\u7f51\u7edc\u8bf7\u6c42\u5931\u8d25 (") +
                QString::number(static_cast<int>(::GetLastError())) + QStringLiteral(")");
        }
        return false;
    }

    DWORD status = 0;
    DWORD statusSize = sizeof(status);
    if (!::WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX, &status, &statusSize, WINHTTP_NO_HEADER_INDEX)) {
        if (error) *error = QString::fromWCharArray(L"\u65e0\u6cd5\u8bfb\u53d6\u670d\u52a1\u5668\u54cd\u5e94");
        return false;
    }
    *statusCode = status;
    if (status != 200) {
        if (error) {
            *error = QString::fromWCharArray(L"\u670d\u52a1\u5668\u8fd4\u56de\u9519\u8bef (HTTP ") +
                QString::number(static_cast<int>(status)) + QStringLiteral(")");
        }
        return false;
    }

    qint64 total = -1;
    wchar_t lengthBuffer[32] = {};
    DWORD lengthSize = sizeof(lengthBuffer);
    if (::WinHttpQueryHeaders(request, WINHTTP_QUERY_CONTENT_LENGTH, WINHTTP_HEADER_NAME_BY_INDEX,
            lengthBuffer, &lengthSize, WINHTTP_NO_HEADER_INDEX)) {
        total = QString::fromWCharArray(lengthBuffer).toLongLong();
    }

    std::vector<char> buffer(64 * 1024);
    qint64 received = 0;
    for (;;) {
        DWORD available = 0;
        if (!::WinHttpQueryDataAvailable(request, &available)) {
            if (error) {
                *error = QString::fromWCharArray(L"\u4e0b\u8f7d\u4e2d\u65ad (") +
                    QString::number(static_cast<int>(::GetLastError())) + QStringLiteral(")");
            }
            return false;
        }
        if (available == 0) {
            break;
        }
        const DWORD chunk = available < static_cast<DWORD>(buffer.size())
            ? available
            : static_cast<DWORD>(buffer.size());
        DWORD readBytes = 0;
        if (!::WinHttpReadData(request, buffer.data(), chunk, &readBytes) || readBytes == 0) {
            if (error) {
                *error = QString::fromWCharArray(L"\u4e0b\u8f7d\u6570\u636e\u8bfb\u53d6\u5931\u8d25");
            }
            return false;
        }
        body->append(buffer.data(), static_cast<int>(readBytes));
        received += readBytes;
        if (onProgress && !onProgress(received, total)) {
            if (error) *error = QString::fromWCharArray(L"\u4e0b\u8f7d\u5df2\u53d6\u6d88");
            return false;
        }
    }
    return true;
}

void UpdateService::checkForUpdate() {
    runWorker([this]() {
        QByteArray body;
        DWORD status = 0;
        QString error;
        if (!fetchUrl(QString::fromLatin1(kReleasesApiUrl), nullptr, &body, &status, &error)) {
            emit checkFinished(false, error, false, QString(), QString(), QString());
            return;
        }

        QJsonParseError parseError{};
        const QJsonDocument document = QJsonDocument::fromJson(body, &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isArray()) {
            emit checkFinished(false,
                QString::fromWCharArray(L"\u7ebf\u4e0a\u7248\u672c\u4fe1\u606f\u89e3\u6790\u5931\u8d25"),
                false, QString(), QString(), QString());
            return;
        }

        QString bestVersion;
        QString bestAssetUrl;
        QString bestNotes;
        for (const QJsonValue& releaseValue : document.array()) {
            const QJsonObject release = releaseValue.toObject();
            const QString version = normalizedVersion(
                release.value(QStringLiteral("tag_name")).toString());
            if (version.isEmpty()) {
                continue;
            }
            if (!bestVersion.isEmpty() && !isVersionNewer(version, bestVersion)) {
                continue;
            }
            bestVersion = version;
            // 发行版描述即用户可读的更新日志。
            bestNotes = release.value(QStringLiteral("body")).toString().trimmed();
            // Prefer an attached zip asset; Gitee also lists auto-generated
            // source archives, which are not usable update packages.
            bestAssetUrl.clear();
            for (const QJsonValue& assetValue : release.value(QStringLiteral("assets")).toArray()) {
                const QString assetUrl = assetValue.toObject()
                    .value(QStringLiteral("browser_download_url")).toString();
                if (!assetUrl.endsWith(QStringLiteral(".zip"), Qt::CaseInsensitive)) {
                    continue;
                }
                if (!assetUrl.contains(QStringLiteral("/releases/download/"), Qt::CaseInsensitive)) {
                    continue;
                }
                bestAssetUrl = assetUrl;
                break;
            }
        }

        if (bestVersion.isEmpty()) {
            emit checkFinished(true,
                QString::fromWCharArray(L"\u7ebf\u4e0a\u6682\u65e0\u53d1\u5e03\u7248\u672c"),
                false, QString(), QString(), QString());
            return;
        }
        if (!isVersionNewer(bestVersion, QString::fromLatin1(APP_VERSION))) {
            emit checkFinished(true,
                QString::fromWCharArray(L"\u5df2\u662f\u6700\u65b0\u7248\u672c"),
                false, bestVersion, QString(), QString());
            return;
        }
        emit checkFinished(true, QString::fromWCharArray(L"\u53d1\u73b0\u65b0\u7248\u672c"),
            true, bestVersion, bestAssetUrl, bestNotes);
    });
}

void UpdateService::downloadAndStageUpdate(const QString& assetUrl, const QString& version) {
    runWorker([this, assetUrl, version]() {
        const QString stagingRoot = QDir::temp().filePath(QStringLiteral("XiaoJiAutoKeyUpdate"));
        QDir(stagingRoot).removeRecursively();
        if (!QDir().mkpath(stagingRoot)) {
            emit stageFinished(false,
                QString::fromWCharArray(L"\u65e0\u6cd5\u521b\u5efa\u4e0b\u8f7d\u4e34\u65f6\u76ee\u5f55"),
                QString());
            return;
        }

        emit downloadProgress(0, 0);
        QByteArray body;
        DWORD status = 0;
        QString error;
        if (!fetchUrl(assetUrl, [this](qint64 received, qint64 total) {
                emit downloadProgress(received, total);
                return true;
            }, &body, &status, &error)) {
            emit stageFinished(false,
                QString::fromWCharArray(L"\u4e0b\u8f7d\u5931\u8d25: ") + error, QString());
            return;
        }

        const QString zipPath = stagingRoot + QStringLiteral("/update-") + version + QStringLiteral(".zip");
        QFile zipFile(zipPath);
        if (!zipFile.open(QIODevice::WriteOnly)) {
            emit stageFinished(false,
                QString::fromWCharArray(L"\u65e0\u6cd5\u4fdd\u5b58\u4e0b\u8f7d\u7684\u66f4\u65b0\u5305"),
                QString());
            return;
        }
        const qint64 written = zipFile.write(body);
        zipFile.close();
        if (written != body.size()) {
            emit stageFinished(false,
                QString::fromWCharArray(L"\u65e0\u6cd5\u4fdd\u5b58\u4e0b\u8f7d\u7684\u66f4\u65b0\u5305"),
                QString());
            return;
        }

        const QString packageDir = stagingRoot + QStringLiteral("/pkg");
        if (!QDir().mkpath(packageDir)) {
            emit stageFinished(false,
                QString::fromWCharArray(L"\u65e0\u6cd5\u521b\u5efa\u89e3\u538b\u76ee\u5f55"),
                QString());
            return;
        }
        const QString extractError = extractZipToDir(zipPath, packageDir);
        if (!extractError.isEmpty()) {
            emit stageFinished(false, extractError, QString());
            return;
        }
        if (!QFileInfo::exists(packageDir + QStringLiteral("/XiaoJiAutoKey.exe"))) {
            emit stageFinished(false,
                QString::fromWCharArray(L"\u66f4\u65b0\u5305\u7f3a\u5c11\u4e3b\u7a0b\u5e8f"),
                QString());
            return;
        }

        emit stageFinished(true,
            QString::fromWCharArray(L"\u66f4\u65b0\u5305\u5df2\u5c31\u7eea\uff0c\u91cd\u542f\u540e\u5b8c\u6210\u66f4\u65b0"),
            packageDir);
    });
}

bool UpdateService::applyStagedUpdate(const QString& stagingDir, QString* errorMessage) {
    const QString exeDir = QCoreApplication::applicationDirPath();
    const QString exePath = exeDir + QStringLiteral("/XiaoJiAutoKey.exe");
    const QString oldExePath = exeDir + QStringLiteral("/XiaoJiAutoKey.old.exe");

    auto fail = [errorMessage](const QString& message) {
        if (errorMessage) {
            *errorMessage = message;
        }
        return false;
    };

    if (!QFileInfo::exists(stagingDir + QStringLiteral("/XiaoJiAutoKey.exe"))) {
        return fail(QString::fromWCharArray(L"\u66f4\u65b0\u5305\u7f3a\u5c11\u4e3b\u7a0b\u5e8f"));
    }

    // Remove leftovers from earlier updates. The old exe may still be held by
    // the previous process for a short moment, so retry a few times.
    for (int attempt = 0; attempt < 5; ++attempt) {
        if (!QFileInfo::exists(oldExePath)) {
            break;
        }
        if (QFile::remove(oldExePath)) {
            break;
        }
        QThread::msleep(150);
    }
    if (QFileInfo::exists(oldExePath)) {
        return fail(QString::fromWCharArray(
            L"\u65e0\u6cd5\u6e05\u7406\u65e7\u7248\u672c\u6587\u4ef6\uff0c\u8bf7\u5148\u5173\u95ed\u5176\u4ed6\u5c0f\u53fd\u6309\u952e\u7a97\u53e3\u540e\u91cd\u8bd5"));
    }

    // Move the running exe away; Windows allows renaming a running image.
    if (!::MoveFileExW(reinterpret_cast<const wchar_t*>(exePath.utf16()),
            reinterpret_cast<const wchar_t*>(oldExePath.utf16()), MOVEFILE_REPLACE_EXISTING)) {
        return fail(QString::fromWCharArray(L"\u65e0\u6cd5\u66ff\u6362\u5f53\u524d\u7a0b\u5e8f\uff0c\u8bf7\u4ee5\u7ba1\u7406\u5458\u8eab\u4efd\u91cd\u8bd5"));
    }
    const auto restoreOldExe = [&exePath, &oldExePath]() {
        ::MoveFileExW(reinterpret_cast<const wchar_t*>(oldExePath.utf16()),
            reinterpret_cast<const wchar_t*>(exePath.utf16()), MOVEFILE_REPLACE_EXISTING);
    };

    bool exeCopied = false;
    QDirIterator it(stagingDir, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString sourcePath = it.next();
        const QString relativePath = sourcePath.mid(stagingDir.size() + 1);
        if (relativePath.compare(QStringLiteral("config.json"), Qt::CaseInsensitive) == 0 ||
            relativePath.endsWith(QStringLiteral(".old.exe"), Qt::CaseInsensitive)) {
            continue;
        }
        const QString targetPath = exeDir + QLatin1Char('/') + relativePath;
        if (!QDir().mkpath(QFileInfo(targetPath).absolutePath())) {
            restoreOldExe();
            return fail(QString::fromWCharArray(L"\u66f4\u65b0\u6587\u4ef6\u5199\u5165\u5931\u8d25: ") + relativePath);
        }
        if (!::CopyFileW(reinterpret_cast<const wchar_t*>(sourcePath.utf16()),
                reinterpret_cast<const wchar_t*>(targetPath.utf16()), FALSE)) {
            restoreOldExe();
            return fail(QString::fromWCharArray(L"\u66f4\u65b0\u6587\u4ef6\u5199\u5165\u5931\u8d25: ") + relativePath +
                QString::fromWCharArray(L"\uff0c\u5df2\u6062\u590d\u539f\u7248\u672c"));
        }
        if (relativePath.compare(QStringLiteral("XiaoJiAutoKey.exe"), Qt::CaseInsensitive) == 0) {
            exeCopied = true;
        }
    }

    if (!exeCopied) {
        restoreOldExe();
        return fail(QString::fromWCharArray(L"\u66f4\u65b0\u5305\u7f3a\u5c11\u4e3b\u7a0b\u5e8f"));
    }
    return true;
}

void UpdateService::cleanupPreviousUpdateArtifacts() {
    const QString oldExePath =
        QCoreApplication::applicationDirPath() + QStringLiteral("/XiaoJiAutoKey.old.exe");
    for (int attempt = 0; attempt < 10 && QFileInfo::exists(oldExePath); ++attempt) {
        if (QFile::remove(oldExePath)) {
            break;
        }
        QThread::msleep(200);
    }
    QDir(QDir::temp().filePath(QStringLiteral("XiaoJiAutoKeyUpdate"))).removeRecursively();
}

void UpdateService::retryOldExeCleanupInBackground(QObject* context) {
    const QString oldExePath = QCoreApplication::applicationDirPath()
        + QStringLiteral("/XiaoJiAutoKey.old.exe");
    if (!QFileInfo::exists(oldExePath)) {
        return;
    }
    auto* timer = new QTimer(context);
    timer->setInterval(3000);
    // 最多重试约 3 分钟：覆盖旧进程退出慢、杀软扫描短暂占用文件等情况。
    int attempts = 0;
    QObject::connect(timer, &QTimer::timeout, timer,
        [timer, oldExePath, attempts]() mutable {
            ++attempts;
            if (!QFileInfo::exists(oldExePath) || QFile::remove(oldExePath) || attempts >= 60) {
                timer->stop();
                timer->deleteLater();
            }
        });
    timer->start();
}

QString UpdateService::normalizedVersion(const QString& version) {
    QString text = version.trimmed();
    if (text.startsWith(QLatin1Char('v'), Qt::CaseInsensitive)) {
        text.remove(0, 1);
    }
    const QStringList rawParts = text.split(QLatin1Char('.'), Qt::SkipEmptyParts);
    if (rawParts.isEmpty()) {
        return {};
    }
    QStringList numericParts;
    for (const QString& rawPart : rawParts) {
        // Drop pre-release suffixes such as "1.2.0-beta".
        const QString numeric = rawPart.section(QLatin1Char('-'), 0, 0);
        bool ok = false;
        const int value = numeric.toInt(&ok);
        if (!ok || value < 0) {
            return {};
        }
        numericParts.push_back(QString::number(value));
    }
    return numericParts.join(QLatin1Char('.'));
}

bool UpdateService::isVersionNewer(const QString& candidate, const QString& reference) {
    const QString candidateNormalized = normalizedVersion(candidate);
    const QString referenceNormalized = normalizedVersion(reference);
    if (candidateNormalized.isEmpty() || referenceNormalized.isEmpty()) {
        return false;
    }
    const QStringList candidateParts = candidateNormalized.split(QLatin1Char('.'));
    const QStringList referenceParts = referenceNormalized.split(QLatin1Char('.'));
    const int count = qMax(candidateParts.size(), referenceParts.size());
    for (int i = 0; i < count; ++i) {
        const int candidateValue = i < candidateParts.size() ? candidateParts.at(i).toInt() : 0;
        const int referenceValue = i < referenceParts.size() ? referenceParts.at(i).toInt() : 0;
        if (candidateValue != referenceValue) {
            return candidateValue > referenceValue;
        }
    }
    return false;
}

QString UpdateService::releasePageUrl() {
    return QStringLiteral("https://gitee.com/fourteen-fourteen/xiao-ji-auto-key/releases");
}
