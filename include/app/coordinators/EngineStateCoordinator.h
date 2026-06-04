#pragma once

#include "AppTypes.h"

#include <QObject>

#include <functional>

class GlobalInputHook;
class MacroEngine;
class MainWindowView;
class RepeaterEngine;

class EngineStateCoordinator : public QObject {
    Q_OBJECT

public:
    struct Context {
        MainWindowView* view = nullptr;
        RepeaterEngine* repeaterEngine = nullptr;
        MacroEngine* macroEngine = nullptr;
        GlobalInputHook* inputHook = nullptr;
        std::function<ProfileConfig*()> activeProfile;
        std::function<MacroConfig*()> activeMacro;
        std::function<void()> refreshHookState;
        std::function<void(bool, bool)> playToggleSound;
    };

    explicit EngineStateCoordinator(QObject* parent = nullptr);

    void bind(Context context);
    void startThreads();
    void stopThreads();
    void toggleRepeatRunning();
    void setRepeatRunning(bool running);
    [[nodiscard]] bool setRepeatRunningFast(bool running);
    void applyBindings();
    void applyMacros();
    void updateRepeatStatus();
    void updateModeSwitchStyle();

    [[nodiscard]] bool canSwitchMode() const;
    [[nodiscard]] bool isAnyEngineRunning() const;

private:
    void handleRepeatStateChanged(bool running);

    Context context_;
};
