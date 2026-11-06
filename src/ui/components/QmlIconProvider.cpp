#include "QmlIconProvider.h"

#include <QColor>
#include <QPainter>
#include <QSvgRenderer>
#include <QDebug>

QmlIconProvider::QmlIconProvider() : QQuickImageProvider(QQuickImageProvider::Image) {}

QImage QmlIconProvider::requestImage(const QString& id, QSize* size, const QSize& requestedSize) {
    const QString path = id.section('/', 0, -2);
    const QColor color(QStringLiteral("#") + id.section('/', -1));
    QSvgRenderer renderer(QStringLiteral(":/assets/%1").arg(path));
    if (!renderer.isValid() || !color.isValid()) {
        qWarning() << "Invalid QML icon:" << id;
        return {};
    }
    const QSize intrinsicSize = renderer.defaultSize();
    if (size) *size = intrinsicSize;
    const QSize imageSize = requestedSize.isValid() ? requestedSize : intrinsicSize;
    // Supersample small SVG strokes even on screens with a device pixel ratio of 1.
    QImage image(imageSize * 2, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    {
        QPainter painter(&image);
        painter.setRenderHint(QPainter::Antialiasing);
        renderer.render(&painter);
        painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
        painter.fillRect(image.rect(), color);
    }
    return image.scaled(imageSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
}
