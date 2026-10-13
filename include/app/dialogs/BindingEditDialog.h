#pragma once

#include "dialogs/KeyCaptureDialogBase.h"

#include <functional>

class ClickPositionGroup;
class FluentComboBox;
class FluentLineEdit;
class FluentSwitch;

struct BindingEditResult {
    KeyDescriptor key;
    RepeatMode mode = RepeatMode::Toggle;
    int delayMs = kDefaultDelayMs;
    bool enabled = true;
    bool suppressMixedMode = false;
    ClickPosition clickPos;
};

class BindingEditDialog : public KeyCaptureDialogBase {
    Q_OBJECT

public:
    BindingEditDialog(std::optional<BindingEditResult> existing,
        int defaultDelayMs,
        std::function<bool()> driverSupportsMouseMove,
        QWidget* parent = nullptr);

    ~BindingEditDialog() override;

    [[nodiscard]] BindingEditResult editResult() const;

private:
    void updateMixedModeSuppressionVisible();
    void updateClickPosSectionVisible();

    FluentComboBox* modeCombo_ = nullptr;
    FluentLineEdit* delayEdit_ = nullptr;
    FluentSwitch* enabledSwitch_ = nullptr;
    QWidget* mixedModeSuppressionRow_ = nullptr;
    FluentSwitch* mixedModeSuppressionSwitch_ = nullptr;

    ClickPositionGroup* clickPosGroup_ = nullptr;

    std::function<bool()> driverSupportsMouseMove_;
};
