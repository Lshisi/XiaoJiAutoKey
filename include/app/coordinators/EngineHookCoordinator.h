#pragma once

#include "AppTypes.h"
#include "controllers/ControllerResult.h"

#include <QObject>
#include <functional>
#include <optional>

class GlobalInputHook;
class MacroEngine;
class RepeaterEngine;
class QWidget;

class EngineHookCoordinator : public QObject {
    Q_OBJECT

public:
    struct Context {
        GlobalInputHook* hook = nullptr;
        RepeaterEngine* repeaterEngine = nullptr;
        MacroEngine* macroEngine = nullptr;
        std::function<void()> toggleRepeatRunning;
        std::function<std::optional<KeyDescriptor>()> controlHotkey;
        std::function<std::optional<KeyDescriptor>()> macroTriggerKey;
        std::function<bool()> captureActive;
        std::function<bool()> repeatRunning;
        std::function<void(quint64)> capturedKey;
        std::function<ControllerResult()> validateActiveConfig;
        QWidget* parent = nullptr;
    };

    explicit EngineHookCoordinator(QObject* parent = nullptr);

    void bind(Context context);
    bool start();
    void refreshState();

private:
    Context context_;
    bool conflictPromptVisible_ = false;
};
