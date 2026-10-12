#pragma once

#include "dialogs/KeyCaptureDialogBase.h"

#include <functional>

class FluentButton;
class FluentComboBox;
class FluentLineEdit;
class FluentSwitch;
class GameWindowProvider;
class QLabel;
class QRadioButton;
class QTimer;

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
        GameWindowProvider* gameWindows,
        std::function<bool()> driverSupportsMouseMove,
        QWidget* parent = nullptr);

    ~BindingEditDialog() override;

    [[nodiscard]] BindingEditResult editResult() const;

private slots:
    void advancePickCountdown();

private:
    void updateMixedModeSuppressionVisible();
    void updateClickPosSectionVisible();
    void beginPickPosition();
    void finishPickPosition();

    FluentComboBox* modeCombo_ = nullptr;
    FluentLineEdit* delayEdit_ = nullptr;
    FluentSwitch* enabledSwitch_ = nullptr;
    QWidget* mixedModeSuppressionRow_ = nullptr;
    FluentSwitch* mixedModeSuppressionSwitch_ = nullptr;

    QWidget* clickPosGroup_ = nullptr;
    QRadioButton* clickFollowRadio_ = nullptr;
    QRadioButton* clickFixedRadio_ = nullptr;
    FluentButton* pickPosBtn_ = nullptr;
    QLabel* pickPosLabel_ = nullptr;
    QLabel* clickPosHintLabel_ = nullptr;
    QTimer* pickTimer_ = nullptr;
    int pickCountdown_ = 0;

    GameWindowProvider* gameWindows_ = nullptr;
    std::function<bool()> driverSupportsMouseMove_;
    ClickPosition clickPos_;
};
