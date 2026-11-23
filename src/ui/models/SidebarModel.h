#pragma once

#include <QAbstractListModel>
#include <QStringList>
#include <QVariantList>

class SidebarModel final : public QAbstractListModel {
    Q_OBJECT
public:
    explicit SidebarModel(QObject* parent = nullptr);
    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    QStringList pageOrder() const { return pageOrder_; }
    void restorePageOrder(const QStringList& order);
    void setPluginPages(const QVariantList& pages);
    Q_INVOKABLE int indexOfPage(const QString& pageId) const;
    Q_INVOKABLE void movePage(const QString& pageId, int targetIndex);

signals:
    void pageOrderChanged();

private:
    enum Role { PageIdRole = Qt::UserRole + 1, TitleRole, IconRole };
    void updateEntries();
    QVariantList builtinPages_;
    QVariantList pluginPages_;
    QVariantList entries_;
    QStringList pageOrder_;
};
