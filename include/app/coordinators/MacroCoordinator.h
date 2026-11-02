#pragma once

#include "AppTypes.h"
#include "controllers/ControllerResult.h"

#include <QObject>
#include <cstdint>
#include <functional>

class GlobalInputHook;
class MacroController;
class MacroEngine;
class MacroPageModel;
class QWidget;
class KeyCaptureDialogBase;

class MacroCoordinator : public QObject {
    Q_OBJECT

public:
    struct Context {
        QWidget* parent = nullptr;
        MacroPageModel* page = nullptr;
        MacroController* controller = nullptr;
        MacroEngine* engine = nullptr;
        GlobalInputHook* inputHook = nullptr;
        std::function<void(KeyCaptureDialogBase&)> runKeyCaptureDialog;
        std::function<void()> applyMacros;
        std::function<void()> saveConfig;
        std::function<void()> refreshMacroPage;
        std::function<void()> refreshMacroProfileCombo;
        std::function<void()> refreshStatusIndicators;
        std::function<void(bool)> setRepeatRunning;
        std::function<void(bool)> playToggleSound;
        std::function<void(bool)> setMacroBeepEnabled;
        std::function<void(bool)> setStatusOverlayEnabled;
        std::function<ProfileConfig*()> activeProfile;
        std::function<ControllerResult()> validateActiveConfig;
        std::function<bool()> canUseDdDriver;
        std::function<void()> showDdUnavailableMessage;
        std::function<bool()> driverSupportsMouseMove;
    };

    explicit MacroCoordinator(QObject* parent = nullptr);

    void bind(Context context);
    void pollRunningState();

private:
    void onMacroSwitched(std::uint64_t macroId);
    void onMacroAdd();
    void onMacroDuplicate();
    void onMacroRename();
    void onNoteEdit();
    void onMacroDelete();
    void onMacroEnabledToggled(bool enabled);
    void onMacroAddStep();
    void onMacroEditStep(int stepIndex);
    void onMacroDeleteStep(int stepIndex);
    void onMacroMoveStep(int fromIndex, int toIndex);
    void onMacroStartStopClicked();
    void onMacroHotkeyClearClicked();
    void onMacroModeChanged(int modeIndex);
    void onMacroLoopCountChanged(int count);
    void onMacroBeepToggled(bool enabled);
    void onStatusOverlayToggled(bool enabled);
    void onPauseKeyAdd();
    void onPauseKeyDelete(int index);
    void stopAllForConfigChange();

    Context context_;
    bool lastMacroRunning_ = false;
};
