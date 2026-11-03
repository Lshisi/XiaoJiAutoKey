#pragma once
#include "dialogs/KeyCaptureDialogBase.h"
class MacroStepDialog : public KeyCaptureDialogBase {
    Q_OBJECT
public:
    MacroStepDialog(std::optional<MacroStep> existing, std::function<bool()> driverSupportsMouseMove, QWidget* parent = nullptr);
    MacroStep editResult() const;
protected:
    void editField(const QString& field, const QVariant& value) override;
    void handleAction(const QString& action, const QVariant& value) override;
};
