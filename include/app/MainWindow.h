#pragma once

#include "AppTypes.h"
#include "DdDriver.h"
#include "input/GlobalInputHook.h"
#include "MacroEngine.h"
#include "controllers/MacroController.h"
#include "view/MainWindowView.h"
#include "controllers/ProfileController.h"
#include "RepeaterEngine.h"
#include "services/SoundService.h"
#include "coordinators/SettingsCoordinator.h"
#include "coordinators/ProfileCoordinator.h"
#include "coordinators/MacroCoordinator.h"
#include "coordinators/KeyBindingsCoordinator.h"
#include "coordinators/EngineHookCoordinator.h"
#include "coordinators/SoundCoordinator.h"
#include "coordinators/CaptureCoordinator.h"
#include "coordinators/PageRefreshCoordinator.h"
#include "coordinators/CloseCoordinator.h"
#include "coordinators/ConfigCoordinator.h"
#include "coordinators/EngineStateCoordinator.h"
#include "services/AppPaths.h"

#include <QMainWindow>

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

    bool Initialize();

protected:
    void closeEvent(QCloseEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void setupUi();
    void setupHooks();

    MainWindowView view_;

    // Core
    AppPaths appPaths_;
    ConfigCoordinator configCoordinator_;
    AppConfig config_;
    ProfileController profileController_;
    MacroController macroController_;

    DdDriver driver_;
    RepeaterEngine engine_;
    MacroEngine macroEngine_;
    GlobalInputHook inputHook_;
    SoundService soundService_;
    SettingsCoordinator settingsCoordinator_;
    ProfileCoordinator profileCoordinator_;
    MacroCoordinator macroCoordinator_;
    KeyBindingsCoordinator keyBindingsCoordinator_;
    EngineHookCoordinator engineHookCoordinator_;
    SoundCoordinator soundCoordinator_;
    CaptureCoordinator captureCoordinator_;
    PageRefreshCoordinator pageRefreshCoordinator_;
    CloseCoordinator closeCoordinator_;
    EngineStateCoordinator engineStateCoordinator_;

};
