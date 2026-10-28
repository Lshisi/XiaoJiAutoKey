#pragma once
#include "dialogs/QmlDialog.h"
class AppMessageDialog : public QmlDialog {
    Q_OBJECT
public:
    enum class Kind { Warning, Information };
    AppMessageDialog(Kind kind, const QString& title, const QString& message,
        const QString& primaryButtonText = QString::fromWCharArray(L"确定"),
        const QString& secondaryButtonText = QString(), QWidget* parent = nullptr);
    static void warning(QWidget* parent, const QString& title, const QString& message);
    static void information(QWidget* parent, const QString& title, const QString& message);
    static bool informationWithActions(QWidget* parent, const QString& title, const QString& message,
        const QString& primaryButtonText, const QString& secondaryButtonText);
protected:
    void handleAction(const QString& action, const QVariant& value) override;
};
