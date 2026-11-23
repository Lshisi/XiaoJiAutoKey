#include "SidebarModel.h"
#include <QVariantMap>

SidebarModel::SidebarModel(QObject* parent) : QAbstractListModel(parent) {
    builtinPages_ = {
        QVariantMap{{"id", "keys"}, {"title", tr("按键连发")}, {"icon", "keyboard"}},
        QVariantMap{{"id", "macro"}, {"title", tr("循环连发")}, {"icon", "repeat"}},
        QVariantMap{{"id", "usage"}, {"title", tr("使用统计")}, {"icon", "bar-chart"}},
        QVariantMap{{"id", "driver"}, {"title", tr("驱动管理")}, {"icon", "monitor"}},
        QVariantMap{{"id", "plugins"}, {"title", tr("插件中心")}, {"icon", "puzzle"}},
        QVariantMap{{"id", "settings"}, {"title", tr("全局设置")}, {"icon", "settings"}},
        QVariantMap{{"id", "about"}, {"title", tr("关于软件")}, {"icon", "info"}}
    };
    updateEntries();
}

int SidebarModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : static_cast<int>(entries_.size());
}

QVariant SidebarModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= rowCount()) return {};
    const auto entry = entries_[index.row()].toMap();
    switch (role) {
    case PageIdRole: return entry.value("id");
    case TitleRole: return entry.value("title");
    case IconRole: return entry.value("icon");
    default: return {};
    }
}

QHash<int, QByteArray> SidebarModel::roleNames() const {
    return {{PageIdRole, "pageId"}, {TitleRole, "title"}, {IconRole, "icon"}};
}

void SidebarModel::restorePageOrder(const QStringList& order) {
    pageOrder_ = order;
    pageOrder_.removeDuplicates();
    pageOrder_.removeAll(QString());
    updateEntries();
}

void SidebarModel::setPluginPages(const QVariantList& pages) {
    QVariantList navigationPages;
    for (const auto& value : pages) {
        const auto page = value.toMap();
        navigationPages.append(QVariantMap{{"id", page.value("id")}, {"title", page.value("title")}, {"icon", page.value("icon")}});
    }
    if (pluginPages_ == navigationPages) return;
    pluginPages_ = navigationPages;
    updateEntries();
}

void SidebarModel::updateEntries() {
    QVariantList available = builtinPages_;
    available.append(pluginPages_);
    bool orderChanged = false;
    for (const auto& value : available) {
        const auto id = value.toMap().value("id").toString();
        if (!pageOrder_.contains(id)) {
            pageOrder_.append(id);
            orderChanged = true;
        }
    }
    QVariantList entries;
    for (const auto& id : pageOrder_) {
        for (const auto& value : available) {
            if (value.toMap().value("id").toString() != id) continue;
            entries.append(value);
            break;
        }
    }
    if (entries_ != entries) {
        beginResetModel();
        entries_ = entries;
        endResetModel();
    }
    if (orderChanged) emit pageOrderChanged();
}

int SidebarModel::indexOfPage(const QString& pageId) const {
    for (int index = 0; index < rowCount(); ++index)
        if (entries_[index].toMap().value("id").toString() == pageId) return index;
    return -1;
}

void SidebarModel::movePage(const QString& pageId, int targetIndex) {
    if (targetIndex < 0 || targetIndex >= rowCount()) return;
    const int sourceIndex = indexOfPage(pageId);
    if (sourceIndex < 0 || sourceIndex == targetIndex) return;
    const auto targetId = entries_[targetIndex].toMap().value("id").toString();
    if (!beginMoveRows({}, sourceIndex, sourceIndex, {}, targetIndex + (targetIndex > sourceIndex ? 1 : 0))) return;
    entries_.move(sourceIndex, targetIndex);
    pageOrder_.removeAll(pageId);
    pageOrder_.insert(pageOrder_.indexOf(targetId) + (targetIndex > sourceIndex ? 1 : 0), pageId);
    endMoveRows();
    emit pageOrderChanged();
}
