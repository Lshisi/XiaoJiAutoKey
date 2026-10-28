#pragma once
#include "dialogs/QmlDialog.h"
class ConfigConfirmDialog : public QmlDialog {
public:
    ConfigConfirmDialog(const QString& title, const QString& message, const QString& confirmText, QWidget* parent = nullptr);
    static bool confirm(QWidget* parent, const QString& title, const QString& message, const QString& confirmText);
protected:
    void handleAction(const QString& action, const QVariant& value) override;
};
