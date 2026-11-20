#pragma once
#include "dialogs/QmlDialog.h"
#include <QStringList>
class GameProcessDialog : public QmlDialog {
public:
    explicit GameProcessDialog(const QStringList& initialProcesses, QWidget* parent = nullptr);
    QStringList processes() const;
protected:
    void handleAction(const QString& action, const QVariant& value) override;
    void editField(const QString& field, const QVariant& value) override;
    void showEvent(QShowEvent* event) override;
private:
    void refreshWindows();
    quint64 iconRevision_ = 0;
};
