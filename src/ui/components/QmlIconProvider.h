#pragma once

#include <QQuickImageProvider>

class QmlIconProvider final : public QQuickImageProvider {
public:
    QmlIconProvider();
    QImage requestImage(const QString& id, QSize* size, const QSize& requestedSize) override;
};
