#pragma once

#include "dialogs/QmlDialog.h"

class PageModel;

class KeyOverviewDialog final : public QmlDialog {
    Q_OBJECT
public:
    KeyOverviewDialog(PageModel& page, bool macroMode, QWidget* parent = nullptr);

signals:
    void navigationRequested(const QVariantMap& target);

protected:
    void handleAction(const QString& action, const QVariant& value) override;
    void editField(const QString& field, const QVariant& value) override;

private:
    void refreshOverview();
    PageModel& page_;
    bool macroMode_;
    QString selectedId_;
    QVariantMap overview_;
};
