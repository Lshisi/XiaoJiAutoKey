#pragma once

#include <QPointer>
#include <QQuickItem>
#include <QUrl>
#include <QtQml/qqmlregistration.h>

class QQmlEngine;

class PluginPageHost : public QQuickItem {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QUrl source READ source WRITE setSource NOTIFY sourceChanged)
    Q_PROPERTY(QObject* api READ api WRITE setApi NOTIFY apiChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
public:
    explicit PluginPageHost(QQuickItem* parent = nullptr);
    ~PluginPageHost() override;
    QUrl source() const { return source_; }
    QObject* api() const { return api_; }
    QString error() const { return error_; }
    void setSource(const QUrl& source);
    void setApi(QObject* api);
signals:
    void sourceChanged();
    void apiChanged();
    void errorChanged();
protected:
    void componentComplete() override;
    void geometryChange(const QRectF& now, const QRectF& before) override;
private:
    void reload();
    void scheduleReload();
    void clear();
    QUrl source_;
    QPointer<QObject> api_;
    QQmlEngine* engine_ = nullptr;
    QQuickItem* content_ = nullptr;
    QString error_;
    bool complete_ = false;
    bool reloadPending_ = false;
    QMetaObject::Connection destroyedConnection_;
};
