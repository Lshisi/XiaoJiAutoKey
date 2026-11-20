#pragma once

#include "AppTypes.h"
#include "controllers/ControllerResult.h"

#include <QObject>
#include <functional>
#include <optional>

class GlobalInputHook;
class MacroEngine;
class RawInputManager;
class RepeaterEngine;
class QWidget;
struct RepeaterStatus;

class EngineHookCoordinator : public QObject {
    Q_OBJECT

public:
    struct Context {
        GlobalInputHook* hook = nullptr;
        RawInputManager* rawInput = nullptr;
        std::function<void(const KeyDescriptor&, bool)> notePhysicalKey;
        RepeaterEngine* repeaterEngine = nullptr;
        MacroEngine* macroEngine = nullptr;
        std::function<void()> toggleRepeatRunning;
        std::function<std::optional<KeyDescriptor>()> controlHotkey;
        std::function<std::optional<KeyDescriptor>()> macroTriggerKey;
        std::function<bool()> captureActive;
        std::function<bool()> repeatRunning;
        std::function<void(quint64)> capturedKey;
        std::function<ControllerResult()> validateActiveConfig;
        std::function<void()> macroStateChanged;
        std::function<bool()> canUseDdDriver;
        std::function<void()> showDdUnavailableMessage;
        std::function<bool()> selfInjectedMouseVisible;
        QWidget* parent = nullptr;
        std::function<bool()> onboardingActive;
        std::function<void(const RepeaterStatus&, const RepeaterStatus&)> repeatInputChanged;
    };

    explicit EngineHookCoordinator(QObject* parent = nullptr);

    void bind(Context context);
    bool start();
    void refreshState();

private:
    void syncInputModes();
    void handleRawInputChanged(const KeyDescriptor& key, bool pressed);
    void handlePhysicalChanged(const KeyDescriptor& key, bool pressed);
    void handleControlHotkeyPressed();
    void handleMacroTriggerChanged(const KeyDescriptor& key, bool pressed);

    Context context_;
    bool conflictPromptVisible_ = false;
    bool rawControlHotkeyPressed_ = false;
};
