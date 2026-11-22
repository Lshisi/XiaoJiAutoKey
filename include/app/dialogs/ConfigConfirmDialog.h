#pragma once
#include "dialogs/QmlDialog.h"
class ConfigConfirmDialog : public QmlDialog {
public:
    ConfigConfirmDialog(const QString& title, const QString& message, const QString& confirmText, QWidget* parent = nullptr,
        const QString& optionText = {}, bool optionChecked = true);
    static bool confirm(QWidget* parent, const QString& title, const QString& message, const QString& confirmText);
    static bool confirmWithOption(QWidget* parent, const QString& title, const QString& message,
        const QString& confirmText, const QString& optionText, bool* checked);
protected:
    void handleAction(const QString& action, const QVariant& value) override;
    void editField(const QString& field, const QVariant& value) override;
};
