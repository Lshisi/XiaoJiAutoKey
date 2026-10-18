#pragma once

#include "AppTypes.h"
#include "DdDriver.h"
#include "ForegroundWatcher.h"
#include "InputDriverRouter.h"
#include "ShareCodec.h"
#include "input/GlobalInputHook.h"
#include "input/RawInputManager.h"
#include "MacroEngine.h"
#include "Ch552UsbDriver.h"
#include "controllers/MacroController.h"
#include "view/MainWindowView.h"
#include "controllers/ProfileController.h"
#include "RepeaterEngine.h"
#include "services/DdRuntimeService.h"
#include "services/SoundService.h"
#include "coordinators/DriverCoordinator.h"
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
#include "coordinators/DriverSelfCheckCoordinator.h"
#include "coordinators/UpdateCoordinator.h"
#include "services/AppPaths.h"
#include "services/AutoStartService.h"
#include "services/UpdateService.h"
#include "widgets/StatusOverlayWindow.h"

#include <QMainWindow>
#include <QStringList>
#include <QTimer>

class QMimeData;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

    bool Initialize();
    void ShowForStartup();

protected:
    void closeEvent(QCloseEvent *event) override;
    void resizeEvent(QResizeEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;
    bool event(QEvent* event) override;
    bool nativeEvent(const QByteArray& eventType, void* message, long* result) override;

private:
    void setupUi();
    void setupHooks();
    void refreshStatusOverlay();
    void openStatusOverlaySettings(bool macroPage);
    bool loadConfiguredDriver(bool allowHidFallbackToLegacy = true);
    bool loadHidDriver();
    bool loadLegacyDriver();
    bool loadCh552UsbDriver();
    bool activeInputDriverReady() const;
    std::wstring activeInputDriverError() const;
    bool loadLegacyDriverInto(DdDriver& targetDriver, std::wstring* errorMessage = nullptr);
    bool canLoadCh552UsbDriver(std::wstring* errorMessage = nullptr);
    bool ensureLegacyDdInfInstalled(std::wstring* resultMessage = nullptr);
    bool ensureHidDriverInstalledForSwitch();
    void logDriverUnavailableOnce();
    void refreshDriverStatusUi();
    DriverCoordinator::SwitchResult switchDriverMode(DriverMode mode);
    void prepareDriverUninstall(DriverMode mode);
    void finalizeDriverUninstall(DriverMode mode);
    bool restartForDriverModeChange();
    bool restartApplication(const QString& reasonMessage);
    bool rebootComputer();
    void showDdUnavailableMessage();
    void scheduleRuntimeVisualRefresh(bool dpiChanged, bool themeChanged);
    void applyRuntimeVisualRefresh();
    void bindDpiRefreshSignals();
    int currentScreenDpi() const;
    void refreshIfScreenDpiChanged();
    void enforceNativeWindowSizeForDpi(int dpi);
    void syncQtGeometryWithNativeWindow();
    void recreateWindowSurfaceAfterDpiChange(int dpi);
    void refreshUsbDriverAfterDeviceChange();
    void registerUsbDeviceNotifications();
    void unregisterUsbDeviceNotifications();
    void prepareForSystemShutdown();
    void applyStagedUpdateAndRestart(const QString& stagingDir);

    // 前台门控（仅游戏客户端前台时生效）
    void applyGameGateSettings();
    // 开机自启（HKCU Run 注册表键）
    void applyAutoStartSettings();
    void onGameForegroundChanged(bool gameForeground);

    // 配置分享
    void openExportProfileDialog();
    void openExportMacroDialog();
    void openImportDialog(const QString& shareText);
    void checkClipboardForShareText();
    bool acceptShareTextDrag(const QMimeData* mimeData);
    // 已提示/已处理过的分享内容本次运行不再重复提示（含导出复制、取消导入的内容）。
    bool isShareTextSuppressed(const QString& text) const;
    void suppressShareText(const QString& text);
    void applyImportedSharePayload(const SharePayload& payload);

    MainWindowView view_;

    // Core
    AppPaths appPaths_;
    ConfigCoordinator configCoordinator_;
    AppConfig config_;
    ProfileController profileController_;
    MacroController macroController_;

    DdDriver driver_;
    Ch552UsbDriver ch552Driver_;
    InputDriverRouter inputDriver_;
    RepeaterEngine engine_;
    MacroEngine macroEngine_;
    GlobalInputHook inputHook_;
    RawInputManager rawInput_;
    ForegroundWatcher gameWatcher_;
    DdRuntimeService ddRuntimeService_;
    SoundService soundService_;
    StatusOverlayWindow statusOverlay_;
    DriverCoordinator driverCoordinator_;
    DriverSelfCheckCoordinator driverSelfCheckCoordinator_;
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
    UpdateService updateService_;
    UpdateCoordinator updateCoordinator_;
    QTimer runtimeVisualRefreshTimer_;
    QTimer usbDeviceRefreshTimer_;
    QTimer usbHeartbeatTimer_;
    void* usbDeviceNotification_ = nullptr;
    void* usbComPortNotification_ = nullptr;
    bool runtimeDpiRefreshPending_ = false;
    bool runtimeThemeRefreshPending_ = false;
    bool runtimeDpiRefreshActive_ = false;
    int runtimeVisualRefreshPassesRemaining_ = 0;
    int runtimeDpiSyncWaitsRemaining_ = 0;
    bool applyingRuntimeVisualRefresh_ = false;
    int observedScreenDpi_ = 0;
    int runtimeDpiOverride_ = 0;
    bool recreatedFractionalDpiSurface_ = false;
    bool suppressDdUnavailablePrompt_ = false;
    bool driverUnavailableLogged_ = false;
    bool driverStatusFallback_ = false;
    // 启动时命中“驱动加载失败 + 内存完整性运行中”的实锤根因，待主窗口首次显示后自动打开自检向导。
    bool pendingSelfCheckPrompt_ = false;
    std::wstring driverStatusNote_;
    bool systemShutdownRequested_ = false;
    bool systemShutdownSaveAttempted_ = false;
    // 剪贴板导入提示抑制记录（新内容在前，超出上限淘汰最旧的）。
    QStringList suppressedShareTexts_;

};
