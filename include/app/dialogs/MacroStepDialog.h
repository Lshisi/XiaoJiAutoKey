#pragma once
#include "dialogs/KeyCaptureDialogBase.h"
struct MacroStepEditResult {
    KeyDescriptor key;
    int delayAfterMs = kDefaultDelayMs;
    ClickPosition clickPos;
};
class MacroStepDialog : public KeyCaptureDialogBase {
    Q_OBJECT
public:
    MacroStepDialog(std::optional<MacroStepEditResult> existing, std::function<bool()> driverSupportsMouseMove, QWidget* parent = nullptr);
    MacroStepEditResult editResult() const;
};
