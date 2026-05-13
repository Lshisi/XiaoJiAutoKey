#pragma once

#include "AppTypes.h"
#include "ConfigStore.h"
#include "DdDriver.h"
#include "RepeaterEngine.h"

#include <QMainWindow>
#include <QStackedWidget>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QLabel>

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
    void updateStatusText();
    void handleCapturedKey(quint64 encodedKey);
    void handleToolStateChanged(bool running);

signals:
    void capturedKey(quint64 encodedKey);
    void holdStateChanged();
    void toolStateChanged(bool running);

private:
    struct CaptureRequest {
        enum class Target { MainHotkey, EditDialogKey };
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
    void playToggleSound(bool running) const;
    std::filesystem::path resolveAudioPath(const wchar_t* fileName) const;
    std::optional<std::size_t> findBindingIndex(std::uint64_t id) const;
    [[nodiscard]] std::wstring getExeDir() const;
    [[nodiscard]] std::filesystem::path getDefaultConfigPath() const;
    [[nodiscard]] std::filesystem::path getConfigPath() const;
    void saveDefaultConfigPointer() const;
    [[nodiscard]] bool IsHoldBindingEnabledForKey(const KeyDescriptor& key) const;
    [[nodiscard]] bool IsHoldBindingEnabledForKeyRealtime(const KeyDescriptor& key) const;

    void openEditDialog(std::optional<std::uint64_t> bindingId);
    void onCapturedKeyInternal(const KeyDescriptor& key);
    void refreshHookState();
    void refreshAllPages();
    void navigateTo(const QString& pageId);

    static LRESULT CALLBACK KeyboardHookProc(int code, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK MouseHookProc(int code, WPARAM wParam, LPARAM lParam);

    // UI Components
    FluentSidebar* sidebar_;
    QStackedWidget* pageStack_;
    KeyPage* keyPage_;
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
    std::uint64_t nextBindingId_ = 1;

    DdDriver driver_;
    RepeaterEngine engine_;

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

    std::thread hookThread_;
    std::atomic<DWORD> hookThreadId_{0};
    std::mutex hookInitMutex_;
    std::condition_variable hookInitCv_;
    bool hookInitCompleted_ = false;
    bool hookInitSucceeded_ = false;
};
