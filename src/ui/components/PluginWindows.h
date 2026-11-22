#pragma once

#include "core/plugins/PluginManifest.h"
#include <QObject>
#include <QPointer>
#include <QTimer>
#include <map>

class PluginPageContext;
class QQmlEngine;
class QQuickWindow;
class QScreen;

namespace Plugins {
class PluginWindows final : public QObject {
    Q_OBJECT
public:
    PluginWindows(Manifest manifest, PluginPageContext* api, QQmlEngine* engine, QVariantMap saved, QObject* parent);
    ~PluginWindows() override;
    bool dispatch(const QString& operation, const QString& id, const QVariantMap& options = {});
    void closeAll();
    QVariantList presentation() const;
    QString error() const { return error_; }

signals:
    void changed();
    void stateSaved(const QVariantMap& state);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void close(const QString& id);
    void remember(const QString& id);
    void constrain(QQuickWindow* window);
    void watchScreen(QScreen* screen);
    Manifest manifest_;
    PluginPageContext* api_;
    QQmlEngine* engine_;
    QVariantMap saved_;
    std::map<QString, QQuickWindow*> windows_;
    QTimer saveTimer_;
    QString error_;
    bool closing_ = false;
};
}
