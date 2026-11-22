#pragma once

#include "PageModel.h"
#include <functional>

class PluginCenterModel final : public PageModel {
    Q_OBJECT
public:
    explicit PluginCenterModel(QObject* parent = nullptr);
    void setSnapshot(const QVariantList& installed, const QVariantList& discover, const QVariantList& pages, bool expanded);
    void setStatus(const QString& message, bool busy = false);
    void setBusy(bool busy) { setValue("busy", busy); }
    void setRegistryConfigured(bool configured) { setValue("registryConfigured", configured); }
    void setPageContextProvider(std::function<QObject*(const QString&)> provider) { contextProvider_ = std::move(provider); }
    Q_INVOKABLE void dispatch(const QString& action, const QString& id = {}, const QVariant& value = {});
    Q_INVOKABLE QVariantMap page(const QString& id) const;
    Q_INVOKABLE QObject* contextForPage(const QString& id) const;

signals:
    void actionRequested(const QString& action, const QString& id, const QVariant& value);

private:
    std::function<QObject*(const QString&)> contextProvider_;
};
