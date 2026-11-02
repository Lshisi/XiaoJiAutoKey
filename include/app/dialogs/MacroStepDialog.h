#pragma once
#include "dialogs/KeyCaptureDialogBase.h"
struct MacroStepEditResult {
    KeyDescriptor key;
    QString name;
    int delayAfterMs = kDefaultDelayMs;
    ClickPosition clickPos;
};
class MacroStepDialog : public KeyCaptureDialogBase {
    Q_OBJECT
public:
    MacroStepDialog(std::optional<MacroStepEditResult> existing, std::function<bool()> driverSupportsMouseMove, QWidget* parent = nullptr);
    MacroStepEditResult editResult() const;
};
