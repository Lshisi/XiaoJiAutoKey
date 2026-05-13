#include "MainWindow.h"

#include "ui/components/FluentSidebar.h"
#include "ui/components/FluentButton.h"
#include "ui/components/FluentCard.h"
#include "ui/components/FluentLineEdit.h"
#include "ui/components/FluentComboBox.h"
#include "ui/components/FluentSwitch.h"
#include "ui/pages/KeyPage.h"
#include "ui/pages/SettingsPage.h"
#include "ui/pages/AboutPage.h"
#include "ui/styles/DesignTokens.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QCloseEvent>
#include <QDialog>
#include <QApplication>
#include <QMetaObject>
#include <QClipboard>
#include <QDesktopServices>
#include <QUrl>
#include <QFileDialog>
#include <QLabel>
#include <QCheckBox>
#include <QPainter>
#include <QInputDialog>
#include <shlwapi.h>
#include <ShlObj.h>
#include <mmsystem.h>
#include <windowsx.h>
#include <dwmapi.h>
#include <array>
#include <fstream>
#include <stdexcept>

#pragma comment(lib, "winmm.lib")

namespace {

MainWindow* g_mainWindow = nullptr;

std::uint64_t EncodeKey(const KeyDescriptor& key) {
    return (static_cast<std::uint64_t>(key.device == InputDevice::Mouse ? 1 : 0) << 32) | key.code;
}

KeyDescriptor DecodeKey(std::uint64_t value) {
    KeyDescriptor key;
    key.device = ((value >> 32) & 0x1) != 0 ? InputDevice::Mouse : InputDevice::Keyboard;
    key.code = static_cast<std::uint32_t>(value & 0xFFFFFFFFull);
    return key;
}

KeyDescriptor MouseMessageToDescriptor(WPARAM wParam, const MSLLHOOKSTRUCT& info) {
    switch (wParam) {
    case WM_LBUTTONDOWN: case WM_LBUTTONUP: return {InputDevice::Mouse, VK_LBUTTON};
    case WM_RBUTTONDOWN: case WM_RBUTTONUP: return {InputDevice::Mouse, VK_RBUTTON};
    case WM_MBUTTONDOWN: case WM_MBUTTONUP: return {InputDevice::Mouse, VK_MBUTTON};
    case WM_XBUTTONDOWN: case WM_XBUTTONUP:
        return {InputDevice::Mouse, static_cast<std::uint32_t>(HIWORD(info.mouseData) == XBUTTON1 ? VK_XBUTTON1 : VK_XBUTTON2)};
    default: return {};
    }
}

bool IsMouseDownMessage(WPARAM wParam) {
    return wParam == WM_LBUTTONDOWN || wParam == WM_RBUTTONDOWN || wParam == WM_MBUTTONDOWN || wParam == WM_XBUTTONDOWN;
}

bool IsMouseUpMessage(WPARAM wParam) {
    return wParam == WM_LBUTTONUP || wParam == WM_RBUTTONUP || wParam == WM_MBUTTONUP || wParam == WM_XBUTTONUP;
}

}  // namespace

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent),
      exeDir_(getExeDir()),
      profileManager_(getDefaultConfigPath().parent_path()),
      configStore_(getDefaultConfigPath()),
      engine_(driver_) {
    g_mainWindow = this;
    profileManager_.Load();
    if (auto* active = profileManager_.ActiveProfile()) {
        configStore_.SetPath(active->configPath);
    }
}

MainWindow::~MainWindow() {
    if (hookThreadId_ != 0) PostThreadMessageW(hookThreadId_, WM_QUIT, 0, 0);
    if (hookThread_.joinable()) hookThread_.join();
    engine_.StopThread();
    if (g_mainWindow == this) g_mainWindow = nullptr;
}

bool MainWindow::Initialize() {
    setupUi();
    loadConfig();

    if (!driver_.LoadFromDirectory(exeDir_.wstring())) {
        std::wstring errorMsg = L"\u9519\u8bef\u539f\u56e0: " + driver_.GetLastError() +
            L"\n\n\u8bf7\u786e\u8ba4\uff1a\n1. \u662f\u5426\u5df2\u5b89\u88c5 DD \u9a71\u52a8\n2. DD64.dll / DD32.dll \u662f\u5426\u5728 exe \u540c\u76ee\u5f55\n\n\u7a0b\u5e8f\u5373\u5c06\u9000\u51fa\u3002";
        QMessageBox::critical(this, QString::fromWCharArray(L"DD \u9a71\u52a8\u52a0\u8f7d\u5931\u8d25"),
            QString::fromStdWString(errorMsg));
        return false;
    }

    try {
        setupHooks();
    } catch (const std::exception&) {
        QMessageBox::critical(this, QString::fromWCharArray(L"\u5168\u5c40\u94a9\u5b50\u521d\u59cb\u5316\u5931\u8d25"),
            QString::fromWCharArray(L"\u65e0\u6cd5\u5b89\u88c5\u5168\u5c40\u952e\u76d8/\u9f20\u6807\u94a9\u5b50"));
        return false;
    }
    engine_.StartThread();
    applyBindingsToEngine();
    refreshAllPages();
    return true;
}

void MainWindow::setupUi() {
    // Use native Windows title bar
    setWindowTitle(QString::fromWCharArray(L"\u5c0f\u53fd\u6309\u952e~"));
    setWindowIcon(QIcon(":/assets/icon.png"));
    resize(780, 560);
    setMinimumSize(720, 500);

    QWidget* centralWidget = new QWidget(this);
    centralWidget->setStyleSheet(QString("background: %1;").arg(Fluent::Colors::pageBg().name()));
    setCentralWidget(centralWidget);

    auto* rootLayout = new QVBoxLayout(centralWidget);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    // Body: Sidebar + Content
    auto* bodyLayout = new QHBoxLayout();
    bodyLayout->setContentsMargins(0, 0, 0, 0);
    bodyLayout->setSpacing(0);

    sidebar_ = new FluentSidebar(this);
    sidebar_->addNavItem(QString::fromWCharArray(L"\u2328"), QString::fromWCharArray(L"\u6309\u952e\u7ba1\u7406"), "keys");
    sidebar_->addNavItem(QString::fromWCharArray(L"\u2699"), QString::fromWCharArray(L"\u5168\u5c40\u8bbe\u7f6e"), "settings");
    sidebar_->addNavItem(QString::fromWCharArray(L"\u2139"), QString::fromWCharArray(L"\u5173\u4e8e\u8f6f\u4ef6"), "about");
    sidebar_->setCurrentPage("keys");
    bodyLayout->addWidget(sidebar_);

    // Page stack
    pageStack_ = new QStackedWidget(this);
    keyPage_ = new KeyPage(this);
    settingsPage_ = new SettingsPage(this);
    aboutPage_ = new AboutPage(this);

    pageStack_->addWidget(keyPage_);
    pageStack_->addWidget(settingsPage_);
    pageStack_->addWidget(aboutPage_);
    pageStack_->setCurrentWidget(keyPage_);
    bodyLayout->addWidget(pageStack_, 1);

    rootLayout->addLayout(bodyLayout, 1);

    // ── Bottom status bar ──
    auto* statusBar = new QWidget(this);
    statusBar->setFixedHeight(24);
    statusBar->setStyleSheet("background: #F5F7FA; border-top: 1px solid rgba(0,0,0,0.08);");
    auto* statusLayout = new QHBoxLayout(statusBar);
    statusLayout->setContentsMargins(12, 0, 12, 0);
    auto* versionLabel = new QLabel(QString("v") + QString(APP_VERSION), statusBar);
    versionLabel->setFont(Fluent::Typography::smallText());
    QPalette stPal = versionLabel->palette();
    stPal.setColor(QPalette::WindowText, Fluent::Colors::textTertiary());
    versionLabel->setPalette(stPal);
    statusLayout->addWidget(versionLabel);
    statusLayout->addStretch();
    auto* authorLabel = new QLabel(QString::fromWCharArray(L"By\uff1a\u9f99\u864e-\u95fb\u6210\u753b"), statusBar);
    authorLabel->setFont(Fluent::Typography::smallText());
    authorLabel->setPalette(stPal);
    statusLayout->addWidget(authorLabel);
    rootLayout->addWidget(statusBar);

    // Sidebar navigation
    connect(sidebar_, &FluentSidebar::pageSelected, this, &MainWindow::navigateTo);

    // KeyPage signals
    connect(keyPage_, &KeyPage::addRequested, this, &MainWindow::onAddBinding);
    connect(keyPage_, &KeyPage::editRequested, this, &MainWindow::onEditBinding);
    connect(keyPage_, &KeyPage::deleteRequested, this, &MainWindow::onDeleteBinding);
    connect(keyPage_, &KeyPage::enableToggled, this, &MainWindow::onEnableToggled);
    connect(keyPage_, &KeyPage::startStopClicked, this, &MainWindow::onStartClicked);
    connect(keyPage_, &KeyPage::hotkeyEditClicked, this, &MainWindow::onHotkeyEditClicked);
    connect(keyPage_, &KeyPage::beepToggled, this, &MainWindow::onBeepToggled);
    connect(keyPage_, &KeyPage::mixedModeToggled, this, &MainWindow::onMixedModeToggled);

    // Profile signals
    connect(keyPage_, &KeyPage::profileSwitched, this, &MainWindow::onProfileSwitched);
    connect(keyPage_, &KeyPage::profileAddRequested, this, &MainWindow::onProfileAdd);
    connect(keyPage_, &KeyPage::profileRenameRequested, this, &MainWindow::onProfileRename);
    connect(keyPage_, &KeyPage::profileDeleteRequested, this, &MainWindow::onProfileDelete);

    // SettingsPage signals
    connect(settingsPage_, &SettingsPage::hotkeyEditClicked, this, &MainWindow::onHotkeyEditClicked);
    connect(settingsPage_, &SettingsPage::beepToggled, this, &MainWindow::onBeepToggled);
    connect(settingsPage_, &SettingsPage::openConfigFolder, this, [this]() {
        const auto folder = configStore_.GetPath().parent_path();
        QDesktopServices::openUrl(QUrl::fromLocalFile(QString::fromStdWString(folder.wstring())));
    });
    connect(settingsPage_, &SettingsPage::changeConfigPath, this, [this]() {
        QString filePath = QFileDialog::getSaveFileName(this,
            QString::fromWCharArray(L"\u9009\u62e9\u914d\u7f6e\u6587\u4ef6\u8def\u5f84"),
            QString::fromStdWString(configStore_.GetPath().wstring()),
            QString::fromWCharArray(L"JSON \u6587\u4ef6 (*.json)"));
        if (filePath.isEmpty()) return;
        std::filesystem::path newPath = filePath.toStdWString();
        std::filesystem::create_directories(newPath.parent_path());
        config_.customConfigPath = newPath.wstring();
        configStore_.SetPath(newPath);
        std::wstring error;
        if (!configStore_.Save(config_, error)) {
            QMessageBox::warning(this, QString::fromWCharArray(L"\u4fdd\u5b58\u5931\u8d25"), QString::fromStdWString(error));
            return;
        }
        profileManager_.UpdateConfigPath(profileManager_.ActiveId(), newPath);
        settingsPage_->setConfigPath(QString::fromStdWString(newPath.wstring()));
    });
    connect(settingsPage_, &SettingsPage::resetConfigPath, this, [this]() {
        std::filesystem::path defaultPath = getDefaultConfigPath();
        if (configStore_.GetPath() == defaultPath) return;
        config_.customConfigPath.clear();
        configStore_.SetPath(defaultPath);
        std::wstring error;
        if (!configStore_.Save(config_, error)) {
            QMessageBox::warning(this, QString::fromWCharArray(L"\u4fdd\u5b58\u5931\u8d25"), QString::fromStdWString(error));
            return;
        }
        profileManager_.UpdateConfigPath(profileManager_.ActiveId(), defaultPath);
        settingsPage_->setConfigPath(QString::fromStdWString(defaultPath.wstring()));
    });
    connect(settingsPage_, &SettingsPage::closeActionChanged, this, [this](CloseAction action) {
        config_.closeAction = action;
        saveConfig();
    });

    // Engine state signals
    connect(this, &MainWindow::capturedKey, this, &MainWindow::handleCapturedKey);
    connect(this, &MainWindow::holdStateChanged, this, &MainWindow::updateStatusText);
    connect(this, &MainWindow::toolStateChanged, this, &MainWindow::handleToolStateChanged);

    // System tray
    trayIcon_ = new QSystemTrayIcon(QIcon(":/assets/icon.png"), this);
    trayMenu_ = new QMenu(this);
    QAction* showAction = trayMenu_->addAction(QString::fromWCharArray(L"\u663e\u793a\u4e3b\u7a97\u53e3"));
    QAction* quitAction = trayMenu_->addAction(QString::fromWCharArray(L"\u9000\u51fa"));
    trayIcon_->setContextMenu(trayMenu_);
    trayIcon_->setToolTip(QString::fromWCharArray(L"\u5c0f\u53fd\u6309\u952e"));
    trayIcon_->show();

    connect(showAction, &QAction::triggered, this, [this]() { showNormal(); activateWindow(); });
    connect(quitAction, &QAction::triggered, this, [this]() { trayIcon_->hide(); qApp->quit(); });
    connect(trayIcon_, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::DoubleClick) { showNormal(); activateWindow(); }
    });
}

void MainWindow::navigateTo(const QString& pageId) {
    if (pageId == "keys") pageStack_->setCurrentWidget(keyPage_);
    else if (pageId == "settings") pageStack_->setCurrentWidget(settingsPage_);
    else if (pageId == "about") pageStack_->setCurrentWidget(aboutPage_);
}

void MainWindow::refreshAllPages() {
    QString hotkey = config_.controlHotkey.has_value()
        ? QString::fromStdWString(GetKeyDisplayName(*config_.controlHotkey))
        : QString();
    keyPage_->setBindings(config_.bindings);
    keyPage_->setRunning(engine_.IsToolRunning());
    keyPage_->setHotkeyText(hotkey.isEmpty() ? QString::fromWCharArray(L"\u672a\u8bbe\u7f6e") : hotkey);
    keyPage_->setBeepEnabled(config_.beepEnabled);
    keyPage_->setMixedMode(config_.mixedMode);
    refreshProfileCombo();
    settingsPage_->setConfig(config_);
    settingsPage_->setHotkeyText(hotkey.isEmpty() ? QString::fromWCharArray(L"\u672a\u8bbe\u7f6e") : hotkey);
    settingsPage_->setConfigPath(QString::fromStdWString(configStore_.GetPath().wstring()));
}

void MainWindow::setupHooks() {
    { std::lock_guard lock(hookInitMutex_); hookInitCompleted_ = false; hookInitSucceeded_ = false; }
    hookThread_ = std::thread(&MainWindow::runHookLoop, this);
    std::unique_lock lock(hookInitMutex_);
    hookInitCv_.wait(lock, [&] { return hookInitCompleted_; });
    if (!hookInitSucceeded_) throw std::runtime_error("failed to install global hooks");
}

void MainWindow::runHookLoop() {
    MSG msg;
    PeekMessageW(&msg, nullptr, WM_USER, WM_USER, PM_NOREMOVE);
    hookThreadId_ = GetCurrentThreadId();
    keyboardHook_ = SetWindowsHookExW(WH_KEYBOARD_LL, &MainWindow::KeyboardHookProc, nullptr, 0);
    mouseHook_ = SetWindowsHookExW(WH_MOUSE_LL, &MainWindow::MouseHookProc, nullptr, 0);
    { std::lock_guard lock(hookInitMutex_); hookInitSucceeded_ = (keyboardHook_ && mouseHook_); hookInitCompleted_ = true; }
    hookInitCv_.notify_one();
    if (!hookInitSucceeded_) {
        if (keyboardHook_) { UnhookWindowsHookEx(keyboardHook_); keyboardHook_ = nullptr; }
        if (mouseHook_) { UnhookWindowsHookEx(mouseHook_); mouseHook_ = nullptr; }
        hookThreadId_ = 0; return;
    }
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    if (keyboardHook_) { UnhookWindowsHookEx(keyboardHook_); keyboardHook_ = nullptr; }
    if (mouseHook_) { UnhookWindowsHookEx(mouseHook_); mouseHook_ = nullptr; }
    hookThreadId_ = 0;
}

void MainWindow::loadConfig() {
    config_ = configStore_.Load();
    nextBindingId_ = 1;
    for (auto& binding : config_.bindings) {
        if (binding.id == 0) { binding.id = nextBindingId_++; continue; }
        nextBindingId_ = std::max(nextBindingId_, binding.id + 1);
    }
    refreshHookState();
}

void MainWindow::saveConfig() {
    std::wstring error;
    if (!configStore_.Save(config_, error)) {
        QMessageBox::warning(this, QString::fromWCharArray(L"\u4fdd\u5b58\u914d\u7f6e\u5931\u8d25"), QString::fromStdWString(error));
    }
}

void MainWindow::applyBindingsToEngine() {
    engine_.UpdateBindings(config_.bindings);
    engine_.SetMixedMode(config_.mixedMode);
    refreshHookState();
    updateStatusText();
}

void MainWindow::updateStatusText() {
    keyPage_->setRunning(engine_.IsToolRunning());
}

void MainWindow::updateHotkeyText() {
    QString text = captureRequest_.has_value() && captureRequest_->target == CaptureRequest::Target::MainHotkey
        ? QString::fromWCharArray(L"\u8bf7\u6309\u4e0b\u4e00\u4e2a\u952e...")
        : (config_.controlHotkey.has_value() ? QString::fromStdWString(GetKeyDisplayName(*config_.controlHotkey)) : QString::fromWCharArray(L"\u672a\u8bbe\u7f6e"));
    keyPage_->setHotkeyText(text);
    settingsPage_->setHotkeyText(text);
}

void MainWindow::updateStartButtonText() { /* handled by KeyPage::setRunning */ }

void MainWindow::toggleToolRunning() { setToolRunning(!engine_.IsToolRunning()); }

void MainWindow::setToolRunning(bool running) {
    if (!setToolRunningFast(running)) return;
    handleToolStateChanged(running);
}

bool MainWindow::setToolRunningFast(bool running) {
    if (engine_.IsToolRunning() == running) return false;
    controlHotkeyPressed_ = false;
    engine_.SetToolRunning(running);
    return true;
}

void MainWindow::handleToolStateChanged(bool running) {
    keyPage_->setRunning(running);
    playToggleSound(running);
}

void MainWindow::playToggleSound(bool running) const {
    if (!config_.beepEnabled) return;
    const auto soundPath = resolveAudioPath(running ? L"\u6309\u952e\u5f00\u542f.mp3" : L"\u6309\u952e\u5173\u95ed.mp3");
    if (soundPath.empty()) return;
    mciSendStringW(L"close autokey_toggle_sound", nullptr, 0, nullptr);
    const std::wstring openCommand = L"open \"" + soundPath.wstring() + L"\" type mpegvideo alias autokey_toggle_sound";
    if (mciSendStringW(openCommand.c_str(), nullptr, 0, nullptr) != 0) return;
    mciSendStringW(L"play autokey_toggle_sound from 0", nullptr, 0, nullptr);
}

std::filesystem::path MainWindow::resolveAudioPath(const wchar_t* fileName) const {
    const auto parent1 = exeDir_.parent_path();
    const auto parent2 = parent1.parent_path();
    const std::array candidates{
        exeDir_ / L"audio" / fileName, exeDir_ / L"assets" / L"audio" / fileName,
        parent1 / L"assets" / L"audio" / fileName, parent2 / L"assets" / L"audio" / fileName,
    };
    for (const auto& c : candidates) { if (!c.empty() && std::filesystem::exists(c)) return c; }
    return {};
}

bool MainWindow::IsHoldBindingEnabledForKey(const KeyDescriptor& key) const {
    return std::any_of(config_.bindings.begin(), config_.bindings.end(), [&](const KeyBinding& b) {
        return b.enabled && b.mode == RepeatMode::Hold && b.key == key;
    });
}

bool MainWindow::IsHoldBindingEnabledForKeyRealtime(const KeyDescriptor& key) const {
    return IsHoldBindingEnabledForKey(key);
}

void MainWindow::onAddBinding() { openEditDialog(std::nullopt); }

void MainWindow::onEditBinding(std::uint64_t id) { openEditDialog(id); }

void MainWindow::onDeleteBinding(std::uint64_t id) {
    config_.bindings.erase(
        std::remove_if(config_.bindings.begin(), config_.bindings.end(),
            [id](const KeyBinding& b) { return b.id == id; }),
        config_.bindings.end());
    keyPage_->setBindings(config_.bindings);
    applyBindingsToEngine();
    saveConfig();
    refreshAllPages();
}

void MainWindow::onEnableToggled(std::uint64_t id, bool enabled) {
    if (auto idx = findBindingIndex(id)) {
        config_.bindings[*idx].enabled = enabled;
        applyBindingsToEngine();
        saveConfig();
    }
}

void MainWindow::onStartClicked() { toggleToolRunning(); }

void MainWindow::onHotkeyEditClicked() {
    captureRequest_ = CaptureRequest{CaptureRequest::Target::MainHotkey};
    refreshHookState();
    { std::lock_guard lock(hookStateMutex_); hookCaptureStartedTick_ = GetTickCount64(); }
    updateHotkeyText();
}

void MainWindow::onBeepToggled(bool enabled) {
    config_.beepEnabled = enabled;
    saveConfig();
}

void MainWindow::onMixedModeToggled(bool enabled) {
    config_.mixedMode = enabled;
    engine_.SetMixedMode(enabled);
    saveConfig();
}

// ── Profile Management ──────────────────────────────────────────────────────

void MainWindow::onProfileSwitched(quint64 profileId) {
    if (profileId == profileManager_.ActiveId()) return;
    // Save current config before switching
    saveConfig();
    switchToProfile(profileId);
}

void MainWindow::onProfileAdd() {
    bool ok = false;
    QString name = QInputDialog::getText(this,
        QString::fromWCharArray(L"\u65b0\u5efa\u914d\u7f6e"),
        QString::fromWCharArray(L"\u8bf7\u8f93\u5165\u914d\u7f6e\u540d\u79f0:"),
        QLineEdit::Normal, QString(), &ok);
    if (!ok || name.trimmed().isEmpty()) return;

    auto entry = profileManager_.AddProfile(name.trimmed().toStdWString());
    switchToProfile(entry.id);
}

void MainWindow::onProfileRename() {
    auto* active = profileManager_.ActiveProfile();
    if (!active) return;

    bool ok = false;
    QString name = QInputDialog::getText(this,
        QString::fromWCharArray(L"\u91cd\u547d\u540d\u914d\u7f6e"),
        QString::fromWCharArray(L"\u65b0\u540d\u79f0:"),
        QLineEdit::Normal, QString::fromStdWString(active->name), &ok);
    if (!ok || name.trimmed().isEmpty()) return;

    profileManager_.RenameProfile(active->id, name.trimmed().toStdWString());
    refreshProfileCombo();
}

void MainWindow::onProfileDelete() {
    auto* active = profileManager_.ActiveProfile();
    if (!active) return;
    if (profileManager_.Profiles().size() <= 1) {
        QMessageBox::information(this,
            QString::fromWCharArray(L"\u65e0\u6cd5\u5220\u9664"),
            QString::fromWCharArray(L"\u81f3\u5c11\u9700\u8981\u4fdd\u7559\u4e00\u4e2a\u914d\u7f6e\u65b9\u6848\u3002"));
        return;
    }

    auto result = QMessageBox::question(this,
        QString::fromWCharArray(L"\u5220\u9664\u914d\u7f6e"),
        QString::fromWCharArray(L"\u786e\u5b9a\u8981\u5220\u9664\u914d\u7f6e\u201c") +
            QString::fromStdWString(active->name) +
            QString::fromWCharArray(L"\u201d\u5417\uff1f\u6b64\u64cd\u4f5c\u4e0d\u53ef\u64a4\u9500\u3002"),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (result != QMessageBox::Yes) return;

    std::uint64_t deletedId = active->id;
    profileManager_.DeleteProfile(deletedId);
    // Switch to the new active profile
    switchToProfile(profileManager_.ActiveId());
}

void MainWindow::switchToProfile(std::uint64_t profileId) {
    // Stop engine before switching
    if (engine_.IsToolRunning()) {
        setToolRunning(false);
    }

    profileManager_.SetActive(profileId);
    auto* profile = profileManager_.ActiveProfile();
    if (!profile) return;

    configStore_.SetPath(profile->configPath);
    loadConfig();
    applyBindingsToEngine();
    refreshAllPages();
}

void MainWindow::refreshProfileCombo() {
    std::vector<std::pair<std::uint64_t, QString>> profiles;
    for (auto& p : profileManager_.Profiles()) {
        profiles.emplace_back(p.id, QString::fromStdWString(p.name));
    }
    keyPage_->setProfiles(profiles, profileManager_.ActiveId());
}

void MainWindow::openEditDialog(std::optional<std::uint64_t> bindingId) {
    if (editDialog_) return;

    QDialog* dialog = new QDialog(this);
    dialog->setWindowTitle(bindingId.has_value()
        ? QString::fromWCharArray(L"\u7f16\u8f91\u6309\u952e")
        : QString::fromWCharArray(L"\u6dfb\u52a0\u6309\u952e"));
    dialog->setFixedSize(420, 300);
    dialog->setWindowFlags(dialog->windowFlags() & ~Qt::WindowContextHelpButtonHint);
    dialog->setStyleSheet("QDialog { background: #FFFFFF; border-radius: 16px; }");

    auto* layout = new QVBoxLayout(dialog);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(16);

    // Key input
    auto* keyRow = new QHBoxLayout();
    auto* keyLabel = new QLabel(QString::fromWCharArray(L"\u6309\u952e:"), dialog);
    keyLabel->setFont(Fluent::Typography::body());
    keyLabel->setFixedWidth(70);
    keyRow->addWidget(keyLabel);
    editKeyBox_ = new FluentLineEdit(dialog);
    editKeyBox_->setReadOnly(true);
    editKeyBox_->setPlaceholderText(QString::fromWCharArray(L"\u70b9\u51fb\u5f55\u5236\u6309\u952e..."));
    keyRow->addWidget(editKeyBox_);
    layout->addLayout(keyRow);

    // Mode
    auto* modeRow = new QHBoxLayout();
    auto* modeLabel = new QLabel(QString::fromWCharArray(L"\u6a21\u5f0f:"), dialog);
    modeLabel->setFont(Fluent::Typography::body());
    modeLabel->setFixedWidth(70);
    modeRow->addWidget(modeLabel);
    editModeCombo_ = new FluentComboBox(dialog);
    editModeCombo_->addItem(QString::fromWCharArray(L"\u5f00\u5173\u6a21\u5f0f"));
    editModeCombo_->addItem(QString::fromWCharArray(L"\u6309\u538b\u6a21\u5f0f"));
    modeRow->addWidget(editModeCombo_);
    layout->addLayout(modeRow);

    // Delay
    auto* delayRow = new QHBoxLayout();
    auto* delayLabel = new QLabel(QString::fromWCharArray(L"\u5ef6\u8fdf(ms):"), dialog);
    delayLabel->setFont(Fluent::Typography::body());
    delayLabel->setFixedWidth(70);
    delayRow->addWidget(delayLabel);
    editDelayEdit_ = new FluentLineEdit(dialog);
    delayRow->addWidget(editDelayEdit_);
    layout->addLayout(delayRow);

    // Enable switch
    auto* enableRow = new QHBoxLayout();
    auto* enableLabel = new QLabel(QString::fromWCharArray(L"\u542f\u7528\u6b64\u6309\u952e"), dialog);
    enableLabel->setFont(Fluent::Typography::body());
    enableRow->addWidget(enableLabel);
    enableRow->addStretch();
    editEnabledSwitch_ = new FluentSwitch(dialog);
    enableRow->addWidget(editEnabledSwitch_);
    layout->addLayout(enableRow);

    layout->addStretch();

    // Buttons
    auto* btnRow = new QHBoxLayout();
    btnRow->addStretch();
    auto* cancelBtn = new FluentButton(FluentButton::Secondary, QString::fromWCharArray(L"\u53d6\u6d88"), dialog);
    cancelBtn->setFixedWidth(80);
    auto* okBtn = new FluentButton(FluentButton::Primary, QString::fromWCharArray(L"\u786e\u5b9a"), dialog);
    okBtn->setFixedWidth(80);
    btnRow->addWidget(cancelBtn);
    btnRow->addSpacing(8);
    btnRow->addWidget(okBtn);
    layout->addLayout(btnRow);

    editDialog_ = dialog;
    editingExisting_ = bindingId.has_value();
    editBindingId_ = bindingId.value_or(0);
    currentEditKey_ = KeyDescriptor{};

    if (editingExisting_) {
        if (auto idx = findBindingIndex(editBindingId_)) {
            const auto& b = config_.bindings[*idx];
            currentEditKey_ = b.key;
            editModeCombo_->setCurrentIndex(b.mode == RepeatMode::Hold ? 1 : 0);
            editDelayEdit_->setText(QString::number(b.delayMs));
            editEnabledSwitch_->setChecked(b.enabled);
        }
    } else {
        editModeCombo_->setCurrentIndex(0);
        editDelayEdit_->setText(QString::number(config_.defaultDelayMs));
        editEnabledSwitch_->setChecked(true);
    }

    editKeyBox_->setText(currentEditKey_.IsValid()
        ? QString::fromStdWString(GetKeyDisplayName(currentEditKey_)) : QString());
    editKeyBox_->installEventFilter(this);

    connect(okBtn, &QPushButton::clicked, dialog, [this, dialog]() {
        if (!currentEditKey_.IsValid()) {
            QMessageBox::warning(dialog, QString::fromWCharArray(L"\u53c2\u6570\u65e0\u6548"),
                QString::fromWCharArray(L"\u8bf7\u5148\u5f55\u5236\u4e00\u4e2a\u6309\u952e\u6216\u9f20\u6807\u952e\u3002"));
            return;
        }
        bool ok; int delay = editDelayEdit_->text().toInt(&ok);
        if (!ok || delay <= 0) {
            QMessageBox::warning(dialog, QString::fromWCharArray(L"\u53c2\u6570\u65e0\u6548"),
                QString::fromWCharArray(L"\u5ef6\u8fdf\u5fc5\u987b\u662f\u5927\u4e8e 0 \u7684\u6574\u6570\u3002"));
            return;
        }
        if (config_.controlHotkey.has_value() && *config_.controlHotkey == currentEditKey_) {
            QMessageBox::warning(dialog, QString::fromWCharArray(L"\u6309\u952e\u51b2\u7a81"),
                QString::fromWCharArray(L"\u8be5\u6309\u952e\u5df2\u88ab\u8bbe\u7f6e\u4e3a\u542f\u505c\u5feb\u6377\u952e\u3002"));
            return;
        }
        for (const auto& b : config_.bindings) {
            if (b.key == currentEditKey_) {
                if (editingExisting_ && b.id == editBindingId_) continue;
                QMessageBox::warning(dialog, QString::fromWCharArray(L"\u6309\u952e\u91cd\u590d"),
                    QString::fromWCharArray(L"\u8be5\u6309\u952e\u5df2\u88ab\u914d\u7f6e\u3002"));
                return;
            }
        }
        if (editingExisting_) {
            if (auto idx = findBindingIndex(editBindingId_)) {
                auto& b = config_.bindings[*idx];
                b.key = currentEditKey_;
                b.mode = editModeCombo_->currentIndex() == 1 ? RepeatMode::Hold : RepeatMode::Toggle;
                b.delayMs = delay;
                b.enabled = editEnabledSwitch_->isChecked();
            }
        } else {
            KeyBinding b;
            b.id = nextBindingId_++;
            b.key = currentEditKey_;
            b.mode = editModeCombo_->currentIndex() == 1 ? RepeatMode::Hold : RepeatMode::Toggle;
            b.delayMs = delay;
            b.enabled = editEnabledSwitch_->isChecked();
            config_.bindings.push_back(b);
        }
        keyPage_->setBindings(config_.bindings);
        applyBindingsToEngine();
        saveConfig();
        refreshAllPages();
        dialog->accept();
    });
    connect(cancelBtn, &QPushButton::clicked, dialog, &QDialog::reject);

    dialog->exec();
    captureRequest_.reset();
    refreshHookState();
    updateHotkeyText();
    editDialog_ = nullptr;
    editKeyBox_ = nullptr;
}

void MainWindow::handleCapturedKey(quint64 encodedKey) { onCapturedKeyInternal(DecodeKey(encodedKey)); }

void MainWindow::onCapturedKeyInternal(const KeyDescriptor& key) {
    if (!captureRequest_.has_value()) return;

    if (captureRequest_->target == CaptureRequest::Target::MainHotkey) {
        captureRequest_.reset();
        refreshHookState();
        if (key.device == InputDevice::Mouse && (key.code == VK_LBUTTON || key.code == VK_RBUTTON)) {
            QMessageBox::warning(this, QString::fromWCharArray(L"\u6309\u952e\u65e0\u6548"),
                QString::fromWCharArray(L"\u542f\u505c\u5feb\u6377\u952e\u4e0d\u80fd\u8bbe\u7f6e\u4e3a\u9f20\u6807\u5de6\u952e\u6216\u53f3\u952e\u3002"));
        } else {
            bool conflict = std::any_of(config_.bindings.begin(), config_.bindings.end(),
                [&](const KeyBinding& b) { return b.key == key; });
            if (conflict) {
                QMessageBox::warning(this, QString::fromWCharArray(L"\u6309\u952e\u51b2\u7a81"),
                    QString::fromWCharArray(L"\u8be5\u6309\u952e\u5df2\u88ab\u914d\u7f6e\u4e3a\u8fde\u53d1/\u6309\u538b\u6309\u952e\u3002"));
            } else {
                config_.controlHotkey = key;
                saveConfig();
                refreshHookState();
            }
        }
        updateHotkeyText();
        refreshAllPages();
    } else {
        currentEditKey_ = key;
        if (editKeyBox_) editKeyBox_->setText(QString::fromStdWString(GetKeyDisplayName(key)));
        captureRequest_.reset();
        refreshHookState();
    }
}

void MainWindow::closeEvent(QCloseEvent *event) {
    saveConfig();
    if (config_.closeAction == CloseAction::Minimize) { hide(); event->ignore(); return; }
    if (config_.closeAction == CloseAction::Exit) { trayIcon_->hide(); event->accept(); return; }

    QDialog dialog(this);
    dialog.setWindowTitle(QString::fromWCharArray(L"\u5173\u95ed\u786e\u8ba4"));
    dialog.setWindowFlags(dialog.windowFlags() & ~Qt::WindowContextHelpButtonHint);
    dialog.setFixedSize(360, 180);
    dialog.setStyleSheet("QDialog { background: #FFFFFF; }");
    auto* layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(24, 20, 24, 16);
    layout->setSpacing(12);
    auto* label = new QLabel(QString::fromWCharArray(L"\u60a8\u5e0c\u671b\u5173\u95ed\u7a0b\u5e8f\u8fd8\u662f\u6700\u5c0f\u5316\u5230\u7cfb\u7edf\u6258\u76d8\uff1f"), &dialog);
    label->setFont(Fluent::Typography::body());
    label->setWordWrap(true);
    layout->addWidget(label);
    auto* rememberCheck = new QCheckBox(QString::fromWCharArray(L"\u8bb0\u4f4f\u6211\u7684\u9009\u62e9"), &dialog);
    rememberCheck->setFont(Fluent::Typography::body());
    layout->addWidget(rememberCheck);
    layout->addStretch();
    auto* btnLayout = new QHBoxLayout();
    btnLayout->addStretch();
    auto* minimizeBtn = new FluentButton(FluentButton::Secondary, QString::fromWCharArray(L"\u6700\u5c0f\u5316\u5230\u6258\u76d8"), &dialog);
    auto* exitBtn = new FluentButton(FluentButton::Primary, QString::fromWCharArray(L"\u9000\u51fa\u7a0b\u5e8f"), &dialog);
    minimizeBtn->setFixedWidth(120); exitBtn->setFixedWidth(100);
    btnLayout->addWidget(minimizeBtn); btnLayout->addSpacing(8); btnLayout->addWidget(exitBtn);
    layout->addLayout(btnLayout);

    bool chosenExit = false;
    connect(minimizeBtn, &QPushButton::clicked, [&]() { chosenExit = false; dialog.accept(); });
    connect(exitBtn, &QPushButton::clicked, [&]() { chosenExit = true; dialog.accept(); });

    // If user clicks X on dialog, just dismiss the dialog (do nothing)
    if (dialog.exec() == QDialog::Rejected) {
        event->ignore();
        return;
    }

    if (rememberCheck->isChecked()) { config_.closeAction = chosenExit ? CloseAction::Exit : CloseAction::Minimize; saveConfig(); }
    if (chosenExit) { trayIcon_->hide(); event->accept(); } else { hide(); event->ignore(); }
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event) {
    if (event->type() == QEvent::MouseButtonPress) {
        if (watched == editKeyBox_) {
            currentEditKey_ = KeyDescriptor{};
            editKeyBox_->setText(QString::fromWCharArray(L"\u8bf7\u6309\u4e0b\u4e00\u4e2a\u952e..."));
            captureRequest_ = CaptureRequest{CaptureRequest::Target::EditDialogKey};
            refreshHookState();
            { std::lock_guard lock(hookStateMutex_); hookCaptureStartedTick_ = GetTickCount64(); }
            return true;
        } else if (captureRequest_.has_value()) {
            captureRequest_.reset();
            refreshHookState();
            updateHotkeyText();
            if (editKeyBox_) editKeyBox_->setText(currentEditKey_.IsValid()
                ? QString::fromStdWString(GetKeyDisplayName(currentEditKey_)) : QString());
        }
    }
    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::refreshHookState() {
    std::lock_guard lock(hookStateMutex_);
    hookControlHotkey_ = config_.controlHotkey;
    hookCaptureActive_ = captureRequest_.has_value();
    if (!hookCaptureActive_) hookCaptureStartedTick_ = 0;
}

std::optional<std::size_t> MainWindow::findBindingIndex(std::uint64_t id) const {
    for (std::size_t i = 0; i < config_.bindings.size(); ++i)
        if (config_.bindings[i].id == id) return i;
    return std::nullopt;
}

std::wstring MainWindow::getExeDir() const {
    std::wstring path(MAX_PATH, L'\0');
    DWORD length = GetModuleFileNameW(nullptr, path.data(), (DWORD)path.size());
    path.resize(length);
    PathRemoveFileSpecW(path.data());
    path.resize(wcslen(path.c_str()));
    return path;
}

std::filesystem::path MainWindow::getDefaultConfigPath() const {
    wchar_t* appDataPath = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &appDataPath))) {
        std::filesystem::path configDir = std::filesystem::path(appDataPath) / L"XiaoJiAutoKey";
        CoTaskMemFree(appDataPath);
        std::filesystem::create_directories(configDir);
        return configDir / L"config.json";
    }
    return std::filesystem::path(exeDir_) / L"config.json";
}

std::filesystem::path MainWindow::getConfigPath() const {
    std::filesystem::path defaultPath = getDefaultConfigPath();
    std::filesystem::path pointerFile = defaultPath.parent_path() / L"config_path.txt";
    if (std::filesystem::exists(pointerFile)) {
        std::wifstream ifs(pointerFile);
        std::wstring customPath;
        if (std::getline(ifs, customPath) && !customPath.empty() && std::filesystem::exists(customPath))
            return customPath;
    }
    return defaultPath;
}

void MainWindow::saveDefaultConfigPointer() const {
    std::filesystem::path defaultPath = getDefaultConfigPath();
    std::filesystem::path pointerFile = defaultPath.parent_path() / L"config_path.txt";
    if (config_.customConfigPath.empty()) std::filesystem::remove(pointerFile);
    else { std::wofstream ofs(pointerFile, std::ios::trunc); ofs << config_.customConfigPath; }
}

// ── Hook Procedures ─────────────────────────────────────────────────────────

LRESULT CALLBACK MainWindow::KeyboardHookProc(int code, WPARAM wParam, LPARAM lParam) {
    if (code < 0 || !g_mainWindow) return CallNextHookEx(nullptr, code, wParam, lParam);
    const auto* info = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);
    KeyDescriptor key{InputDevice::Keyboard, info->vkCode};
    const bool pressed = wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN;
    const bool released = wParam == WM_KEYUP || wParam == WM_SYSKEYUP;
    if (!pressed && !released) return CallNextHookEx(nullptr, code, wParam, lParam);
    if (info->flags & LLKHF_INJECTED) return CallNextHookEx(nullptr, code, wParam, lParam);

    const bool toolRunning = g_mainWindow->engine_.IsToolRunning();
    std::optional<KeyDescriptor> controlHotkey;
    bool captureActive = false;
    { std::lock_guard lock(g_mainWindow->hookStateMutex_); controlHotkey = g_mainWindow->hookControlHotkey_; captureActive = g_mainWindow->hookCaptureActive_; }

    if (captureActive && pressed) {
        auto enc = EncodeKey(key);
        QMetaObject::invokeMethod(g_mainWindow, [w = g_mainWindow, enc] { w->handleCapturedKey(enc); }, Qt::QueuedConnection);
        return 1;
    }
    const bool isCtrl = controlHotkey.has_value() && *controlHotkey == key;
    if (isCtrl) {
        if (pressed && !g_mainWindow->controlHotkeyPressed_.exchange(true)) {
            bool nr = !toolRunning;
            if (g_mainWindow->setToolRunningFast(nr))
                QMetaObject::invokeMethod(g_mainWindow, [w = g_mainWindow, nr] { w->handleToolStateChanged(nr); }, Qt::QueuedConnection);
        } else if (released) g_mainWindow->controlHotkeyPressed_ = false;
    }
    if (toolRunning) { if (pressed) g_mainWindow->engine_.OnPhysicalDown(key); else if (released) g_mainWindow->engine_.OnPhysicalUp(key); }
    return CallNextHookEx(nullptr, code, wParam, lParam);
}

LRESULT CALLBACK MainWindow::MouseHookProc(int code, WPARAM wParam, LPARAM lParam) {
    if (code < 0 || !g_mainWindow) return CallNextHookEx(nullptr, code, wParam, lParam);
    const auto* info = reinterpret_cast<MSLLHOOKSTRUCT*>(lParam);
    if (!IsMouseDownMessage(wParam) && !IsMouseUpMessage(wParam)) return CallNextHookEx(nullptr, code, wParam, lParam);
    const KeyDescriptor key = MouseMessageToDescriptor(wParam, *info);
    if (!key.IsValid()) return CallNextHookEx(nullptr, code, wParam, lParam);
    if (info->flags & LLMHF_INJECTED) return CallNextHookEx(nullptr, code, wParam, lParam);

    const bool pressed = IsMouseDownMessage(wParam);
    const bool released = IsMouseUpMessage(wParam);
    const bool toolRunning = g_mainWindow->engine_.IsToolRunning();
    std::optional<KeyDescriptor> controlHotkey;
    bool captureActive = false;
    ULONGLONG captureStartedTick = 0;
    { std::lock_guard lock(g_mainWindow->hookStateMutex_); controlHotkey = g_mainWindow->hookControlHotkey_; captureActive = g_mainWindow->hookCaptureActive_; captureStartedTick = g_mainWindow->hookCaptureStartedTick_; }

    if (captureActive && pressed) {
        if (GetTickCount64() - captureStartedTick < 150) return CallNextHookEx(nullptr, code, wParam, lParam);
        auto enc = EncodeKey(key);
        QMetaObject::invokeMethod(g_mainWindow, [w = g_mainWindow, enc] { w->handleCapturedKey(enc); }, Qt::QueuedConnection);
        return 1;
    }
    const bool isCtrl = controlHotkey.has_value() && *controlHotkey == key;
    if (isCtrl) {
        if (pressed && !g_mainWindow->controlHotkeyPressed_.exchange(true)) {
            bool nr = !toolRunning;
            if (g_mainWindow->setToolRunningFast(nr))
                QMetaObject::invokeMethod(g_mainWindow, [w = g_mainWindow, nr] { w->handleToolStateChanged(nr); }, Qt::QueuedConnection);
        } else if (released) g_mainWindow->controlHotkeyPressed_ = false;
    }
    if (toolRunning) { if (pressed) g_mainWindow->engine_.OnPhysicalDown(key); else if (released) g_mainWindow->engine_.OnPhysicalUp(key); }
    return CallNextHookEx(nullptr, code, wParam, lParam);
}
