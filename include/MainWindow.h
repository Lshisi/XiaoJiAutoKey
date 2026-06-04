#pragma once

#include "AppTypes.h"
#include "ConfigStore.h"
#include "DdDriver.h"
#include "MacroEngine.h"
#include "RepeaterEngine.h"

#include <QMainWindow>
#include <QStackedWidget>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QLabel>
#include <QPushButton>

#include <Windows.h>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <filesystem>
#include <thread>
#include <vector>

class FluentSidebar;
class KeyPage;
class MacroPage;
class SettingsPage;
class AboutPage;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

    bool Initialize();

protected:
    void closeEvent(QCloseEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void onAddBinding();
    void onEditBinding(std::uint64_t id);
    void onDeleteBinding(std::uint64_t id);
    void onEnableToggled(std::uint64_t id, bool enabled);
    void onStartClicked();
    void onHotkeyEditClicked();
    void onBeepToggled(bool enabled);
    void onMacroBeepToggled(bool enabled);
    void onMixedModeToggled(bool enabled);
    void updateStatusText();
    void handleCapturedKey(quint64 encodedKey);
    void handleToolStateChanged(bool running);

    // Pause key management
    void onPauseKeyAdd();
    void onPauseKeyDelete(int index);
    void onSoundSettingsClicked();

    // Profile management
    void onProfileSwitched(quint64 profileId);
    void onProfileAdd();
    void onProfileRename();
    void onProfileDelete();

    // Macro management
    void onMacroSwitched(quint64 macroId);
    void onMacroAdd();
    void onMacroRename();
    void onMacroDelete();
    void onMacroAddStep();
    void onMacroEditStep(int stepIndex);
    void onMacroDeleteStep(int stepIndex);
    void onMacroMoveStep(int fromIndex, int toIndex);
    void onMacroHotkeyEditClicked();
    void onMacroStartStopClicked();
    void onMacroModeChanged(int modeIndex);
    void onMacroLoopCountChanged(int count);

signals:
    void capturedKey(quint64 encodedKey);
    void holdStateChanged();
    void toolStateChanged(bool running);

private:
    struct CaptureRequest {
        enum class Target { MainHotkey, EditDialogKey, MacroHotkey };
        Target target = Target::MainHotkey;
    };

    void setupUi();
    void setupHooks();
    void runHookLoop();
    void loadConfig();
    void saveConfig();
    void applyBindingsToEngine();
    void updateHotkeyText();
    void updateStartButtonText();
    void toggleToolRunning();
    void setToolRunning(bool running);
    bool setToolRunningFast(bool running);
    void playToggleSound(bool running, bool macroMode = false) const;
    void playSoundFile(const std::filesystem::path& soundPath, int volume, const wchar_t* alias) const;
    std::filesystem::path resolveToggleSoundPath(bool running, bool macroMode = false) const;
    std::filesystem::path resolveAudioPath(const wchar_t* fileName) const;
    std::optional<std::size_t> findBindingIndex(std::uint64_t id) const;
    [[nodiscard]] std::wstring getExeDir() const;
    [[nodiscard]] std::filesystem::path getDefaultConfigPath() const;
    [[nodiscard]] bool IsHoldBindingEnabledForKey(const KeyDescriptor& key) const;
    [[nodiscard]] bool IsHoldBindingEnabledForKeyRealtime(const KeyDescriptor& key) const;

    void openEditDialog(std::optional<std::uint64_t> bindingId);
    void onCapturedKeyInternal(const KeyDescriptor& key);
    void refreshHookState();
    void refreshAllPages();
    void refreshMacroPage();
    void navigateTo(const QString& pageId);
    void refreshProfileCombo();
    void applyMacrosToEngine();
    MacroConfig* findActiveMacro();
    void updateModeSwitchStyle();
    [[nodiscard]] bool isAnyEngineRunning() const;

    // Profile helpers
    ProfileConfig* activeProfile();
    const ProfileConfig* activeProfile() const;
    void switchToProfile(std::uint64_t profileId);
    std::uint64_t nextProfileId() const;
    std::uint64_t nextBindingIdInProfile() const;
    std::uint64_t nextMacroIdInProfile() const;

    static LRESULT CALLBACK KeyboardHookProc(int code, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK MouseHookProc(int code, WPARAM wParam, LPARAM lParam);

    // UI Components
    FluentSidebar* sidebar_;
    QStackedWidget* pageStack_;
    QWidget* keySettingsContainer_;
    QStackedWidget* keyMacroStack_;
    QPushButton* modeRepeatBtn_;
    QPushButton* modeMacroBtn_;
    KeyPage* keyPage_;
    MacroPage* macroPage_;
    SettingsPage* settingsPage_;
    AboutPage* aboutPage_;

    QSystemTrayIcon* trayIcon_ = nullptr;
    QMenu* trayMenu_ = nullptr;

    // Hooks
    HHOOK keyboardHook_ = nullptr;
    HHOOK mouseHook_ = nullptr;

    // Core
    std::filesystem::path exeDir_;
    ConfigStore configStore_;
    AppConfig config_;
    std::uint64_t activeMacroId_ = 0;

    DdDriver driver_;
    RepeaterEngine engine_;
    MacroEngine macroEngine_;

    std::optional<CaptureRequest> captureRequest_;

    // For Edit Dialog
    QWidget* editDialog_ = nullptr;
    class FluentLineEdit* editKeyBox_ = nullptr;
    class FluentComboBox* editModeCombo_ = nullptr;
    class FluentLineEdit* editDelayEdit_ = nullptr;
    class FluentSwitch* editEnabledSwitch_ = nullptr;
    std::uint64_t editBindingId_ = 0;
    bool editingExisting_ = false;
    KeyDescriptor currentEditKey_{};

    std::atomic<bool> controlHotkeyPressed_{false};
    mutable std::mutex hookStateMutex_;
    std::optional<KeyDescriptor> hookControlHotkey_;
    bool hookCaptureActive_ = false;
    ULONGLONG hookCaptureStartedTick_ = 0;
    bool lastMacroRunning_ = false;

    // 弹窗打开时屏蔽启停快捷键和宏触发（atomic，hook线程直接读取）
    std::atomic<bool> dialogOpen_{false};

    std::thread hookThread_;
    std::atomic<DWORD> hookThreadId_{0};
    std::mutex hookInitMutex_;
    std::condition_variable hookInitCv_;
    bool hookInitCompleted_ = false;
    bool hookInitSucceeded_ = false;

    // 当前激活的模式：0=按键连发, 1=宏循环
    std::atomic<int> activeMode_{0};
};
