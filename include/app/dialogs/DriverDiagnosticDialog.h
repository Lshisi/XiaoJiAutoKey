#pragma once

#include "dialogs/QmlDialog.h"

class DriverDiagnosticDialog final : public QmlDialog {
    Q_OBJECT
public:
    explicit DriverDiagnosticDialog(QWidget* parent = nullptr);
    void beginReport();
    void setReport(const QString& report);
protected:
    void handleAction(const QString& action, const QVariant& value) override;
private:
    QString report_;
};
