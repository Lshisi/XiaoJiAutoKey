#include "FactionIcon.h"

#include <QApplication>
#include <QPainter>
#include <QScreen>
#include <QSvgRenderer>
#include <QWidget>
#include <QWindow>

#include <cmath>

namespace {

qreal effectiveDevicePixelRatio(const QWidget* widget) {
    qreal ratio = 1.0;
    if (widget) {
        ratio = qMax(ratio, widget->devicePixelRatioF());
        if (const QWindow* window = widget->windowHandle()) {
            ratio = qMax(ratio, window->devicePixelRatio());
            if (const QScreen* screen = window->screen()) {
                ratio = qMax(ratio, screen->devicePixelRatio());
            }
        }
        if (const QWidget* topLevel = widget->window()) {
            if (const QWindow* handle = topLevel->windowHandle()) {
                ratio = qMax(ratio, handle->devicePixelRatio());
                if (const QScreen* screen = handle->screen()) {
                    ratio = qMax(ratio, screen->devicePixelRatio());
                }
            }
        }
    }
    if (const QScreen* screen = QGuiApplication::primaryScreen()) {
        ratio = qMax(ratio, screen->devicePixelRatio());
    }
    return qMax<qreal>(1.0, ratio);
}

QSize physicalSizeFor(const QSize& logicalSize, qreal devicePixelRatio) {
    return QSize(
        qMax(1, static_cast<int>(std::ceil(logicalSize.width() * devicePixelRatio))),
        qMax(1, static_cast<int>(std::ceil(logicalSize.height() * devicePixelRatio))));
}

QPixmap tintPixmap(const QPixmap& source, const QColor& color) {
    if (source.isNull() || !color.isValid()) {
        return source;
    }

    QPixmap tinted(source.size());
    tinted.setDevicePixelRatio(source.devicePixelRatio());
    tinted.fill(Qt::transparent);

    const qreal ratio = qMax<qreal>(1.0, source.devicePixelRatio());
    const QRectF logicalRect(0, 0, source.width() / ratio, source.height() / ratio);

    QPainter painter(&tinted);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.drawPixmap(logicalRect, source, logicalRect);
    painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
    painter.fillRect(logicalRect, color);
    return tinted;
}

}

namespace FactionIcon {

QPixmap render(const QString& path, const QSize& logicalSize, const QWidget* referenceWidget, const QColor& tint) {
    if (path.isEmpty() || !logicalSize.isValid()) {
        return QPixmap();
    }

    const qreal ratio = effectiveDevicePixelRatio(referenceWidget);
    const QSize physicalSize = physicalSizeFor(logicalSize, ratio);

    QPixmap pixmap;
    if (path.endsWith(QStringLiteral(".svg"), Qt::CaseInsensitive)) {
        QSvgRenderer renderer(path);
        if (!renderer.isValid()) {
            return QPixmap();
        }

        pixmap = QPixmap(physicalSize);
        pixmap.setDevicePixelRatio(ratio);
        pixmap.fill(Qt::transparent);

        QPainter painter(&pixmap);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
        renderer.render(&painter, QRectF(QPointF(0, 0), QSizeF(logicalSize)));
    } else {
        const QPixmap source(path);
        if (source.isNull()) {
            return QPixmap();
        }
        pixmap = source.scaled(physicalSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        pixmap.setDevicePixelRatio(ratio);
    }

    return tintPixmap(pixmap, tint);
}

}
