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
#include "widgets/StatusOverlayWindow.h"

#include <QMainWindow>
#include <QTimer>

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

    bool Initialize();

protected:
    void closeEvent(QCloseEvent *event) override;
    void resizeEvent(QResizeEvent* event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;
    bool event(QEvent* event) override;
    bool nativeEvent(const QByteArray& eventType, void* message, long* result) override;

private:
    void setupUi();
    void setupHooks();
    void refreshStatusOverlay();
    void scheduleRuntimeVisualRefresh(bool dpiChanged, bool themeChanged);
    void applyRuntimeVisualRefresh();
    void bindDpiRefreshSignals();
    int currentScreenDpi() const;
    void refreshIfScreenDpiChanged();
    void enforceNativeWindowSizeForDpi(int dpi);

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
    StatusOverlayWindow statusOverlay_;
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
    QTimer runtimeVisualRefreshTimer_;
    bool runtimeDpiRefreshPending_ = false;
    bool runtimeThemeRefreshPending_ = false;
    int runtimeVisualRefreshPassesRemaining_ = 0;
    bool applyingRuntimeVisualRefresh_ = false;
    int observedScreenDpi_ = 0;
    int runtimeDpiOverride_ = 0;

};
