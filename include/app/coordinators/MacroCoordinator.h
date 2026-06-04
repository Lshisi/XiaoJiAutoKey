#pragma once

#include "AppTypes.h"

#include <QObject>
#include <cstdint>
#include <functional>

class GlobalInputHook;
class MacroController;
class MacroEngine;
class MacroPage;
class QWidget;
class KeyCaptureDialogBase;

class MacroCoordinator : public QObject {
    Q_OBJECT

public:
    struct Context {
        QWidget* parent = nullptr;
        MacroPage* page = nullptr;
        MacroController* controller = nullptr;
        MacroEngine* engine = nullptr;
        GlobalInputHook* inputHook = nullptr;
        std::function<void(KeyCaptureDialogBase&)> runKeyCaptureDialog;
        std::function<void()> applyMacros;
        std::function<void()> saveConfig;
        std::function<void()> refreshMacroPage;
        std::function<void()> updateModeSwitchStyle;
        std::function<void(bool)> playToggleSound;
        std::function<void(bool)> setMacroBeepEnabled;
    };

    explicit MacroCoordinator(QObject* parent = nullptr);

    void bind(Context context);
    void pollRunningState();

private:
    void onMacroSwitched(std::uint64_t macroId);
    void onMacroAdd();
    void onMacroRename();
    void onMacroDelete();
    void onMacroAddStep();
    void onMacroEditStep(int stepIndex);
    void onMacroDeleteStep(int stepIndex);
    void onMacroMoveStep(int fromIndex, int toIndex);
    void onMacroStartStopClicked();
    void onMacroModeChanged(int modeIndex);
    void onMacroLoopCountChanged(int count);
    void onMacroBeepToggled(bool enabled);

    Context context_;
    bool lastMacroRunning_ = false;
};
