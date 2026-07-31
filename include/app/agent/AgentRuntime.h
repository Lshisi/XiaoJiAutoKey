#pragma once

#include "AppTypes.h"
#include "Ch552UsbDriver.h"
#include "ConfigStore.h"
#include "InputDriverRouter.h"
#include "MacroEngine.h"
#include "RepeaterEngine.h"
#include "input/GlobalInputHook.h"
#include "input/RawInputManager.h"
#include "services/SoundService.h"
#include "widgets/StatusOverlayWindow.h"

#include <QTimer>
#include <QWidget>

#include <atomic>
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>

class AgentRuntime final : public QWidget {
    Q_OBJECT

public:
    explicit AgentRuntime(QWidget* parent = nullptr);
    ~AgentRuntime() override;

    bool initialize();

protected:
    bool nativeEvent(const QByteArray& eventType, void* message, long* result) override;

private:
    void pipeLoop();
    std::string handleRequest(const std::string& request);
    void ensureRuntimeState();
    bool refreshConfigPath();
    bool connectUsb();
    void activateRuntime();
    void deactivateRuntime(bool disconnectUsb, bool preserveRepeatState = true);
    void applyConfig();
    void reloadConfig(bool repeatRunning);
    void refreshHookState();
    void updateOverlay();
    void playToggleSound(bool running, bool macroMode);
    void handleInput(const KeyDescriptor& key, bool pressed);
    void handleMacroInput(const KeyDescriptor& key, bool pressed);
    void toggleRepeat();
    void checkFrontendProcess();
    void wakePipeServer();

    ConfigStore store_;
    AppConfig config_;
    Ch552UsbDriver usbDriver_;
    InputDriverRouter inputDriver_;
    RepeaterEngine repeater_;
    MacroEngine macro_;
    GlobalInputHook hook_;
    RawInputManager rawInput_;
    SoundService sound_;
    StatusOverlayWindow overlay_;
    QTimer runtimeTimer_;
    QTimer stateTimer_;
    std::thread pipeThread_;
    std::atomic<bool> pipeExit_{false};
    std::atomic<bool> shutdownRequested_{false};
    bool runtimeActive_ = false;
    bool pausedForUi_ = false;
    bool desiredRepeatRunning_ = false;
    bool controlHotkeyDown_ = false;
    bool firmwareUpdateRequired_ = false;
    bool configPathReady_ = false;
    DWORD frontendPid_ = 0;
};
