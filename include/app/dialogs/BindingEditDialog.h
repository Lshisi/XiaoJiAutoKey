#pragma once

#include "dialogs/KeyCaptureDialogBase.h"

class FluentComboBox;
class FluentLineEdit;
class FluentSwitch;

struct BindingEditResult {
    KeyDescriptor key;
    RepeatMode mode = RepeatMode::Toggle;
    int delayMs = 10;
    bool enabled = true;
};

class BindingEditDialog : public KeyCaptureDialogBase {
    Q_OBJECT

public:
    BindingEditDialog(std::optional<BindingEditResult> existing, int defaultDelayMs, QWidget* parent = nullptr);

    [[nodiscard]] BindingEditResult editResult() const;

private:
    FluentComboBox* modeCombo_ = nullptr;
    FluentLineEdit* delayEdit_ = nullptr;
    FluentSwitch* enabledSwitch_ = nullptr;
};
