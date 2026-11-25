#include "QmlIconProvider.h"

#include <QColor>
#include <QImageReader>
#include <QPainter>
#include <QSvgRenderer>
#include <QDebug>

#ifdef Q_OS_WIN
#include <Windows.h>
#include <objbase.h>
#include <shellapi.h>

namespace {

QImage CopyWindowIcon(HICON icon) {
    if (!icon) return {};
    const HICON copy = ::CopyIcon(icon);
    if (!copy) return {};
    const QImage image = QImage::fromHICON(copy);
    ::DestroyIcon(copy);
    return image;
}

QImage NativeWindowIcon(const QString& id) {
    bool validWindow = false;
    bool validProcess = false;
    const auto handle = id.section('/', 1, 1).toULongLong(&validWindow);
    const DWORD expectedProcess = id.section('/', 2, 2).toUInt(&validProcess);
    const HWND window = reinterpret_cast<HWND>(static_cast<quintptr>(handle));
    DWORD processId = 0;
    if (!validWindow || !validProcess || !::IsWindow(window)
        || ::GetWindowThreadProcessId(window, &processId) == 0 || processId != expectedProcess) return {};

    for (const WPARAM kind : {ICON_BIG, ICON_SMALL2, ICON_SMALL}) {
        DWORD_PTR result = 0;
        if (::SendMessageTimeoutW(window, WM_GETICON, kind, 0,
                SMTO_ABORTIFHUNG | SMTO_BLOCK, 50, &result) != 0 && result != 0) {
            const QImage image = CopyWindowIcon(reinterpret_cast<HICON>(result));
            if (!image.isNull()) return image;
        }
    }
    for (const int kind : {GCLP_HICON, GCLP_HICONSM}) {
        const QImage image = CopyWindowIcon(reinterpret_cast<HICON>(::GetClassLongPtrW(window, kind)));
        if (!image.isNull()) return image;
    }

    const HANDLE process = ::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId);
    if (!process) return {};
    wchar_t path[32768]{};
    DWORD length = 32768;
    const BOOL queried = ::QueryFullProcessImageNameW(process, 0, path, &length);
    ::CloseHandle(process);
    if (!queried) return {};

    const HRESULT initialized = ::CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    SHFILEINFOW info{};
    const auto found = ::SHGetFileInfoW(path, 0, &info, sizeof(info), SHGFI_ICON | SHGFI_LARGEICON);
    QImage image;
    if (found && info.hIcon) image = QImage::fromHICON(info.hIcon);
    if (info.hIcon) ::DestroyIcon(info.hIcon);
    if (SUCCEEDED(initialized)) ::CoUninitialize();
    return image;
}

}  // namespace
#endif

QmlIconProvider::QmlIconProvider() : QQuickImageProvider(QQuickImageProvider::Image) {}

QString QmlIconProvider::pageIconName(const QString& file) {
    return QStringLiteral("plugin-pages/")
        + QString::fromLatin1(file.toUtf8().toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals));
}

QImage QmlIconProvider::requestImage(const QString& id, QSize* size, const QSize& requestedSize) {
    if (id.startsWith(QStringLiteral("windows/"))) {
        QImage image;
#ifdef Q_OS_WIN
        image = NativeWindowIcon(id);
#endif
        if (size) *size = image.size();
        return !image.isNull() && requestedSize.isValid()
            ? image.scaled(requestedSize, Qt::KeepAspectRatio, Qt::SmoothTransformation) : image;
    }
    QString path = id.section('/', 0, -2);
    const bool preserveColors = id.section('/', -1) == QStringLiteral("original");
    const QColor color = preserveColors ? QColor() : QColor(QStringLiteral("#") + id.section('/', -1));
    if (path.startsWith(QStringLiteral("plugin-pages/"))) {
        const auto decoded = QByteArray::fromBase64Encoding(path.section('/', 1).toLatin1(),
            QByteArray::Base64UrlEncoding | QByteArray::AbortOnBase64DecodingErrors);
        if (!decoded) {
            qWarning() << "Invalid plugin page icon:" << id;
            return {};
        }
        path = QString::fromUtf8(decoded.decoded);
    } else {
        path = QStringLiteral(":/assets/%1").arg(path);
    }
    if (!preserveColors && !color.isValid()) {
        qWarning() << "Invalid QML icon:" << id;
        return {};
    }
    QSvgRenderer renderer;
    QImageReader pngReader;
    QSize intrinsicSize;
    const bool png = path.endsWith(QStringLiteral(".png"), Qt::CaseInsensitive);
    if (png) {
        pngReader.setFileName(path);
        pngReader.setFormat(QByteArrayLiteral("png"));
        intrinsicSize = pngReader.size();
    } else if (path.endsWith(QStringLiteral(".svg"), Qt::CaseInsensitive) && renderer.load(path)) {
        intrinsicSize = renderer.defaultSize();
    }
    if (!intrinsicSize.isValid() || intrinsicSize.isEmpty()) {
        qWarning() << "Invalid QML icon file:" << path;
        return {};
    }
    if (size) *size = intrinsicSize;
    const QSize imageSize = requestedSize.isValid() ? requestedSize : intrinsicSize;
    QImage raster;
    if (png) {
        pngReader.setScaledSize(intrinsicSize.scaled(imageSize * 2, Qt::KeepAspectRatio));
        raster = pngReader.read();
        if (raster.isNull()) {
            qWarning() << "Invalid plugin page PNG:" << path << pngReader.errorString();
            return {};
        }
    }
    // Supersample small icon strokes even on screens with a device pixel ratio of 1.
    QImage image(imageSize * 2, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    {
        QPainter painter(&image);
        painter.setRenderHint(QPainter::Antialiasing);
        if (png) {
            const QSize rasterSize = raster.size();
            painter.drawImage(QPoint((image.width() - rasterSize.width()) / 2,
                (image.height() - rasterSize.height()) / 2), raster);
        } else {
            renderer.render(&painter);
        }
        if (!preserveColors) {
            painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
            painter.fillRect(image.rect(), color);
        }
    }
    return image.scaled(imageSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
}
