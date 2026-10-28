#pragma once
#include "dialogs/QmlDialog.h"
class ConfigNameDialog : public QmlDialog {
public:
    ConfigNameDialog(const QString& title, const QString& label, const QString& initialName = {}, QWidget* parent = nullptr);
    QString nameText() const;
    static bool getName(QWidget* parent, const QString& title, const QString& label, const QString& initialName, QString& name);
protected:
    void handleAction(const QString& action, const QVariant& value) override;
    void editField(const QString& field, const QVariant& value) override;
};
