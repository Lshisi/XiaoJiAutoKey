#pragma once
#include "dialogs/KeyCaptureDialogBase.h"
class MacroStepDialog : public KeyCaptureDialogBase {
    Q_OBJECT
public:
    MacroStepDialog(std::optional<MacroStep> existing, std::function<bool()> driverSupportsMouseMove,
        QWidget* parent = nullptr, std::optional<MacroStep> initial = std::nullopt);
    MacroStep editResult() const;
    void setActionGuideCallbacks(std::function<bool(MacroAction)> shouldShow,
        std::function<void(MacroAction)> dismissed);
protected:
    void editField(const QString& field, const QVariant& value) override;
    void handleAction(const QString& action, const QVariant& value) override;
private:
    void updateActionGuide();
    void dismissActionGuide();
    std::function<bool(MacroAction)> shouldShowActionGuide_;
    std::function<void(MacroAction)> actionGuideDismissed_;
};
