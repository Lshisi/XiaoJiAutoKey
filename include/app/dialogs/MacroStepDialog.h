#pragma once

#include "dialogs/KeyCaptureDialogBase.h"

#include <functional>

class ClickPositionGroup;
class FluentLineEdit;

struct MacroStepEditResult {
    KeyDescriptor key;
    int delayAfterMs = kDefaultDelayMs;
    ClickPosition clickPos;
};

class MacroStepDialog : public KeyCaptureDialogBase {
    Q_OBJECT

public:
    MacroStepDialog(std::optional<MacroStepEditResult> existing,
        std::function<bool()> driverSupportsMouseMove,
        QWidget* parent = nullptr);

    [[nodiscard]] MacroStepEditResult editResult() const;

private:
    void updateClickPosSectionVisible();

    FluentLineEdit* delayEdit_ = nullptr;
    ClickPositionGroup* clickPosGroup_ = nullptr;
};
