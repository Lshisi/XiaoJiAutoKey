#include "PluginCenterModel.h"
#include <QQmlEngine>

PluginCenterModel::PluginCenterModel(QObject* parent) : PageModel(parent) {
    state_ = {{"installed", QVariantList{}}, {"discover", QVariantList{}}, {"pages", QVariantList{}},
        {"expanded", true}, {"busy", false}, {"updates", 0}, {"status", QString()}, {"registryConfigured", false}};
}

void PluginCenterModel::setSnapshot(const QVariantList& installed, const QVariantList& discover, const QVariantList& pages, bool expanded) {
    int updates = 0;
    for (const auto& value : installed) if (value.toMap().value("updateAvailable").toBool()) ++updates;
    state_.insert("installed", installed);
    state_.insert("discover", discover);
    state_.insert("pages", pages);
    state_.insert("expanded", expanded);
    state_.insert("updates", updates);
    emit stateChanged();
}

void PluginCenterModel::setStatus(const QString& message, bool busy) {
    if (state_.value("status").toString() == message && state_.value("busy").toBool() == busy) return;
    state_.insert("status", message);
    state_.insert("busy", busy);
    emit stateChanged();
}

void PluginCenterModel::dispatch(const QString& action, const QString& id, const QVariant& value) {
    emit actionRequested(action, id, value);
}

QVariantMap PluginCenterModel::page(const QString& id) const {
    for (const auto& value : state_.value("pages").toList()) {
        const auto record = value.toMap();
        if (record.value("id").toString() == id) return record;
    }
    return {};
}

QObject* PluginCenterModel::contextForPage(const QString& id) const {
    auto* context = contextProvider_ ? contextProvider_(id) : nullptr;
    if (context) QQmlEngine::setObjectOwnership(context, QQmlEngine::CppOwnership);
    return context;
}
