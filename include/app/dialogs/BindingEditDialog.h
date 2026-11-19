#pragma once
#include "dialogs/KeyCaptureDialogBase.h"
struct BindingEditResult {
    KeyDescriptor key;
    QString name;
    RepeatMode mode = RepeatMode::Toggle;
    int delayMs = kDefaultDelayMs;
    bool enabled = true;
    bool suppressMixedMode = false;
    ClickPosition clickPos;
    bool martialAssist = false;
    bool preventAccidentalStop = false;
};
class BindingEditDialog : public KeyCaptureDialogBase {
    Q_OBJECT
public:
    BindingEditDialog(std::optional<BindingEditResult> existing, int defaultDelayMs, bool mixedMode,
        std::function<bool()> driverSupportsMouseMove, QWidget* parent = nullptr);
    BindingEditResult editResult() const;
};
