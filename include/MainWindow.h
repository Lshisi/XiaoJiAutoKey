#pragma once

#include "AppTypes.h"
#include "ConfigStore.h"
#include "DdDriver.h"
#include "RepeaterEngine.h"

#include <QMainWindow>
#include <QTableWidget>
#include <QPushButton>
#include <QLineEdit>
#include <QComboBox>
#include <QCheckBox>
#include <QLabel>
#include <QTabWidget>
#include <QGroupBox>
#include <QSystemTrayIcon>
#include <QMenu>

#include <Windows.h>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <filesystem>
#include <thread>
#include <vector>

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
    void onDeleteSelected();
    void onStartClicked();
    void onHotkeyEditClicked();
    void onBeepToggled();
    void onAboutClicked();
    void updateStatusText();
    void onListDoubleClicked(int row, int col);
    void onItemChanged(QTableWidgetItem* item);
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
    void refreshList();
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
    [[nodiscard]] bool IsHoldBindingEnabledForKey(const KeyDescriptor& key) const;
    [[nodiscard]] bool IsHoldBindingEnabledForKeyRealtime(const KeyDescriptor& key) const;

    void openEditDialog(std::optional<std::uint64_t> bindingId);
    void onCapturedKeyInternal(const KeyDescriptor& key);
    void refreshHookState();

    static LRESULT CALLBACK KeyboardHookProc(int code, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK MouseHookProc(int code, WPARAM wParam, LPARAM lParam);

    QTableWidget* tableWidget_;
    QPushButton* addBtn_;
    QPushButton* deleteBtn_;
    QLineEdit* hotkeyEdit_;
    QCheckBox* beepCheck_;
    QPushButton* startBtn_;
    QLabel* statusLabel_;
    QSystemTrayIcon* trayIcon_ = nullptr;
    QMenu* trayMenu_ = nullptr;

    HHOOK keyboardHook_ = nullptr;
    HHOOK mouseHook_ = nullptr;

    std::filesystem::path exeDir_;
    ConfigStore configStore_;
    AppConfig config_;
    std::uint64_t nextBindingId_ = 1;

    DdDriver driver_;
    RepeaterEngine engine_;

    std::optional<CaptureRequest> captureRequest_;
    
    // For Edit Dialog
    QWidget* editDialog_ = nullptr;
    QLineEdit* editKeyBox_ = nullptr;
    QComboBox* editModeCombo_ = nullptr;
    QLineEdit* editDelayEdit_ = nullptr;
    QCheckBox* editEnabledCheck_ = nullptr;
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
