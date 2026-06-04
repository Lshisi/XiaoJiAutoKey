#pragma once

#include "AppTypes.h"

#include <QObject>
#include <functional>
#include <optional>

class GlobalInputHook;
class MacroEngine;
class RepeaterEngine;

class EngineHookCoordinator : public QObject {
    Q_OBJECT

public:
    struct Context {
        GlobalInputHook* hook = nullptr;
        RepeaterEngine* repeaterEngine = nullptr;
        MacroEngine* macroEngine = nullptr;
        std::function<void()> toggleRepeatRunning;
        std::function<std::optional<KeyDescriptor>()> controlHotkey;
        std::function<bool()> captureActive;
        std::function<int()> activeMode;
        std::function<bool()> repeatRunning;
        std::function<void(quint64)> capturedKey;
    };

    explicit EngineHookCoordinator(QObject* parent = nullptr);

    void bind(Context context);
    bool start();
    void refreshState();

private:
    Context context_;
};
