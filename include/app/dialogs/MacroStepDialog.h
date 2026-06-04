#pragma once

#include "dialogs/KeyCaptureDialogBase.h"

class FluentLineEdit;

struct MacroStepEditResult {
    KeyDescriptor key;
    int delayAfterMs = 50;
};

class MacroStepDialog : public KeyCaptureDialogBase {
    Q_OBJECT

public:
    MacroStepDialog(std::optional<MacroStepEditResult> existing, QWidget* parent = nullptr);

    [[nodiscard]] MacroStepEditResult editResult() const;

private:
    FluentLineEdit* delayEdit_ = nullptr;
};
