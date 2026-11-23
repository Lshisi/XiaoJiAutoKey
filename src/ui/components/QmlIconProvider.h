#pragma once

#include <QQuickImageProvider>

class QmlIconProvider final : public QQuickImageProvider {
public:
    QmlIconProvider();
    static QString pageIconName(const QString& file);
    QImage requestImage(const QString& id, QSize* size, const QSize& requestedSize) override;
};
