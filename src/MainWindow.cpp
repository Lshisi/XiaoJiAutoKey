#include "MainWindow.h"

#include "FactionTheme.h"
#include "MacroEngine.h"
#include "ui/components/FluentSidebar.h"
#include "ui/components/FluentButton.h"
#include "ui/components/FluentCard.h"
#include "ui/components/FluentLineEdit.h"
#include "ui/components/FluentComboBox.h"
#include "ui/components/FluentSwitch.h"
#include "ui/pages/KeyPage.h"
#include "ui/pages/MacroPage.h"
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
#include <QLabel>
#include <QCheckBox>
#include <QPainter>
#include <QInputDialog>
#include <QTimer>
#include <QFileDialog>
#include <QFileInfo>
#include <QLineEdit>
#include <QSlider>
#include <QStringList>
#include <shlwapi.h>
#include <ShlObj.h>
#include <mmsystem.h>
#include <windowsx.h>
#include <dwmapi.h>
#include <array>
#include <algorithm>
#include <fstream>
#include <stdexcept>

#pragma comment(lib, "winmm.lib")

namespace {

MainWindow* g_mainWindow = nullptr;

const QStringList kSupportedAudioExtensions = {
    "mp3", "wav", "wma", "mid", "midi"
};

bool IsValidAudioFile(const QString& path) {
    QFileInfo info(path);
    if (!info.exists() || !info.isFile()) return false;
    return kSupportedAudioExtensions.contains(info.suffix().toLower());
}

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
      configStore_(getDefaultConfigPath()),
      engine_(driver_),
      macroEngine_(driver_) {
    g_mainWindow = this;
}

MainWindow::~MainWindow() {
    if (hookThreadId_ != 0) PostThreadMessageW(hookThreadId_, WM_QUIT, 0, 0);
    if (hookThread_.joinable()) hookThread_.join();
    engine_.StopThread();
    macroEngine_.StopThread();
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
    macroEngine_.StartThread();
    applyBindingsToEngine();
    applyMacrosToEngine();
    refreshAllPages();
    return true;
}

void MainWindow::setupUi() {
    setWindowTitle(QString::fromWCharArray(L"\u5c0f\u53fd\u6309\u952e~"));
    setWindowIcon(QIcon(":/assets/icon.png"));
    resize(780, 560);
    setMinimumSize(720, 560);

    QWidget* centralWidget = new QWidget(this);
    centralWidget->setStyleSheet(QString("background: %1;").arg(Fluent::Colors::pageBg().name()));
    setCentralWidget(centralWidget);

    auto* rootLayout = new QVBoxLayout(centralWidget);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    auto* bodyLayout = new QHBoxLayout();
    bodyLayout->setContentsMargins(0, 0, 0, 0);
    bodyLayout->setSpacing(0);

    sidebar_ = new FluentSidebar(this);
    sidebar_->addNavItem(QString::fromWCharArray(L"\u2328"), QString::fromWCharArray(L"\u6309\u952e\u8bbe\u7f6e"), "keys");
    sidebar_->addNavItem(QString::fromWCharArray(L"\u2699"), QString::fromWCharArray(L"\u5168\u5c40\u8bbe\u7f6e"), "settings");
    sidebar_->addNavItem(QString::fromWCharArray(L"\u2139"), QString::fromWCharArray(L"\u5173\u4e8e\u8f6f\u4ef6"), "about");
    sidebar_->setCurrentPage("keys");
    bodyLayout->addWidget(sidebar_);

    pageStack_ = new QStackedWidget(this);

    // ── 合并的按键设置页面（含切换：按键连发 / 宏循环）──
    keySettingsContainer_ = new QWidget(this);
    auto* ksLayout = new QVBoxLayout(keySettingsContainer_);
    ksLayout->setContentsMargins(0, 0, 0, 0);
    ksLayout->setSpacing(0);

    // 顶部切换栏
    auto* switchBar = new QWidget(keySettingsContainer_);
    switchBar->setFixedHeight(40);
    switchBar->setStyleSheet("background: #FFFFFF; border-bottom: 1px solid rgba(0,0,0,0.06);");
    auto* switchLayout = new QHBoxLayout(switchBar);
    switchLayout->setContentsMargins(16, 4, 16, 4);
    switchLayout->setSpacing(0);

    modeRepeatBtn_ = new QPushButton(QString::fromWCharArray(L"\u6309\u952e\u8fde\u53d1"), switchBar);
    modeMacroBtn_ = new QPushButton(QString::fromWCharArray(L"\u5b8f\u5faa\u73af"), switchBar);
    modeRepeatBtn_->setFixedHeight(30);
    modeMacroBtn_->setFixedHeight(30);
    modeRepeatBtn_->setMinimumWidth(90);
    modeMacroBtn_->setMinimumWidth(90);
    modeRepeatBtn_->setCursor(Qt::PointingHandCursor);
    modeMacroBtn_->setCursor(Qt::PointingHandCursor);

    switchLayout->addWidget(modeRepeatBtn_);
    switchLayout->addWidget(modeMacroBtn_);
    switchLayout->addStretch();

    ksLayout->addWidget(switchBar);

    // 内容切换栈
    keyMacroStack_ = new QStackedWidget(keySettingsContainer_);
    keyPage_ = new KeyPage(this);
    macroPage_ = new MacroPage(this);
    keyMacroStack_->addWidget(keyPage_);
    keyMacroStack_->addWidget(macroPage_);
    keyMacroStack_->setCurrentWidget(keyPage_);
    ksLayout->addWidget(keyMacroStack_, 1);

    pageStack_->addWidget(keySettingsContainer_);

    settingsPage_ = new SettingsPage(this);
    aboutPage_ = new AboutPage(this);
    pageStack_->addWidget(settingsPage_);
    pageStack_->addWidget(aboutPage_);
    pageStack_->setCurrentWidget(keySettingsContainer_);
    bodyLayout->addWidget(pageStack_, 1);

    // 切换按钮样式和逻辑
    updateModeSwitchStyle();
    connect(modeRepeatBtn_, &QPushButton::clicked, this, [this]() {
        if (isAnyEngineRunning()) return;  // 运行时不允许切换
        keyMacroStack_->setCurrentWidget(keyPage_);
        activeMode_.store(0);
        updateModeSwitchStyle();
    });
    connect(modeMacroBtn_, &QPushButton::clicked, this, [this]() {
        if (isAnyEngineRunning()) return;  // 运行时不允许切换
        keyMacroStack_->setCurrentWidget(macroPage_);
        activeMode_.store(1);
        updateModeSwitchStyle();
    });

    rootLayout->addLayout(bodyLayout, 1);

    // Bottom status bar
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
    connect(keyPage_, &KeyPage::pauseKeyAddClicked, this, &MainWindow::onPauseKeyAdd);
    connect(keyPage_, &KeyPage::pauseKeyDeleteClicked, this, &MainWindow::onPauseKeyDelete);
    connect(keyPage_, &KeyPage::soundSettingsClicked, this, &MainWindow::onSoundSettingsClicked);

    // Profile signals
    connect(keyPage_, &KeyPage::profileSwitched, this, &MainWindow::onProfileSwitched);
    connect(keyPage_, &KeyPage::profileAddRequested, this, &MainWindow::onProfileAdd);
    connect(keyPage_, &KeyPage::profileRenameRequested, this, &MainWindow::onProfileRename);
    connect(keyPage_, &KeyPage::profileDeleteRequested, this, &MainWindow::onProfileDelete);

    // MacroPage signals
    connect(macroPage_, &MacroPage::macroSwitched, this, &MainWindow::onMacroSwitched);
    connect(macroPage_, &MacroPage::macroAddRequested, this, &MainWindow::onMacroAdd);
    connect(macroPage_, &MacroPage::macroRenameRequested, this, &MainWindow::onMacroRename);
    connect(macroPage_, &MacroPage::macroDeleteRequested, this, &MainWindow::onMacroDelete);
    connect(macroPage_, &MacroPage::addStepRequested, this, &MainWindow::onMacroAddStep);
    connect(macroPage_, &MacroPage::editStepRequested, this, &MainWindow::onMacroEditStep);
    connect(macroPage_, &MacroPage::deleteStepRequested, this, &MainWindow::onMacroDeleteStep);
    connect(macroPage_, &MacroPage::stepMoveRequested, this, &MainWindow::onMacroMoveStep);
    connect(macroPage_, &MacroPage::startStopClicked, this, &MainWindow::onMacroStartStopClicked);
    connect(macroPage_, &MacroPage::hotkeyEditClicked, this, &MainWindow::onMacroHotkeyEditClicked);
    connect(macroPage_, &MacroPage::beepToggled, this, &MainWindow::onMacroBeepToggled);
    connect(macroPage_, &MacroPage::soundSettingsClicked, this, &MainWindow::onSoundSettingsClicked);
    connect(macroPage_, &MacroPage::modeChanged, this, &MainWindow::onMacroModeChanged);
    connect(macroPage_, &MacroPage::loopCountChanged, this, &MainWindow::onMacroLoopCountChanged);

    // SettingsPage signals
    connect(settingsPage_, &SettingsPage::hotkeyEditClicked, this, &MainWindow::onHotkeyEditClicked);
    connect(settingsPage_, &SettingsPage::openConfigFolder, this, [this]() {
        const auto folder = configStore_.GetPath().parent_path();
        QDesktopServices::openUrl(QUrl::fromLocalFile(QString::fromStdWString(folder.wstring())));
    });
    connect(settingsPage_, &SettingsPage::closeActionChanged, this, [this](CloseAction action) {
        config_.closeAction = action;
        saveConfig();
    });
    connect(settingsPage_, &SettingsPage::factionChanged, this, [this](const QString& factionId) {
        config_.faction = factionId.toStdString();
        FactionTheme::instance().setCurrent(factionId);
        saveConfig();
        sidebar_->updateFactionIcon();
        sidebar_->update();
        for (auto* child : sidebar_->findChildren<QWidget*>()) child->update();
        keyPage_->updateThemeColors();
        keyPage_->update();
        macroPage_->updateThemeColors();
        update();
    });
    // Engine state signals
    connect(this, &MainWindow::capturedKey, this, &MainWindow::handleCapturedKey);
    connect(this, &MainWindow::holdStateChanged, this, &MainWindow::updateStatusText);
    connect(this, &MainWindow::toolStateChanged, this, &MainWindow::handleToolStateChanged);

    // Macro state polling timer
    auto* macroStateTimer = new QTimer(this);
    connect(macroStateTimer, &QTimer::timeout, this, [this]() {
        auto* macro = findActiveMacro();
        if (macro) {
            bool running = macroEngine_.IsRunning(macro->id);
            macroPage_->setRunning(running);
            updateModeSwitchStyle();
            if (running != lastMacroRunning_) {
                lastMacroRunning_ = running;
                playToggleSound(running, true);
            }
        }
    });
    macroStateTimer->start(100);

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
    if (pageId == "keys") pageStack_->setCurrentWidget(keySettingsContainer_);
    else if (pageId == "settings") pageStack_->setCurrentWidget(settingsPage_);
    else if (pageId == "about") pageStack_->setCurrentWidget(aboutPage_);
}

void MainWindow::updateModeSwitchStyle() {
    const bool isRepeat = (keyMacroStack_->currentWidget() == keyPage_);
    const bool running = isAnyEngineRunning();

    QString activeStyle = QString(
        "QPushButton { background: %1; color: white; border: none; border-radius: 6px; "
        "font-size: 13px; font-weight: 500; padding: 4px 16px; }"
    ).arg(FactionTheme::instance().primaryColor().name());

    QString inactiveStyle = QString(
        "QPushButton { background: transparent; color: %1; border: none; border-radius: 6px; "
        "font-size: 13px; padding: 4px 16px; }"
        "QPushButton:hover { background: rgba(0,0,0,0.04); }"
    ).arg(running ? "rgba(0,0,0,0.25)" : "rgba(0,0,0,0.6)");

    QString disabledStyle = QString(
        "QPushButton { background: transparent; color: rgba(0,0,0,0.25); border: none; border-radius: 6px; "
        "font-size: 13px; padding: 4px 16px; }"
    );

    modeRepeatBtn_->setStyleSheet(isRepeat ? activeStyle : (running ? disabledStyle : inactiveStyle));
    modeMacroBtn_->setStyleSheet(!isRepeat ? activeStyle : (running ? disabledStyle : inactiveStyle));

    // 运行时禁用非当前模式的按钮
    modeRepeatBtn_->setEnabled(!running || isRepeat);
    modeMacroBtn_->setEnabled(!running || !isRepeat);
}

bool MainWindow::isAnyEngineRunning() const {
    return engine_.IsToolRunning() || macroEngine_.HasActiveMacro();
}

// ── Profile helpers ─────────────────────────────────────────────────────────

ProfileConfig* MainWindow::activeProfile() {
    for (auto& p : config_.profiles) {
        if (p.id == config_.activeProfileId) return &p;
    }
    return config_.profiles.empty() ? nullptr : &config_.profiles.front();
}

const ProfileConfig* MainWindow::activeProfile() const {
    for (const auto& p : config_.profiles) {
        if (p.id == config_.activeProfileId) return &p;
    }
    return config_.profiles.empty() ? nullptr : &config_.profiles.front();
}

std::uint64_t MainWindow::nextProfileId() const {
    std::uint64_t maxId = 0;
    for (const auto& p : config_.profiles) maxId = std::max(maxId, p.id);
    return maxId + 1;
}

std::uint64_t MainWindow::nextBindingIdInProfile() const {
    std::uint64_t maxId = 0;
    if (auto* p = activeProfile()) {
        for (const auto& b : p->bindings) maxId = std::max(maxId, b.id);
    }
    return maxId + 1;
}

std::uint64_t MainWindow::nextMacroIdInProfile() const {
    std::uint64_t maxId = 0;
    if (auto* p = activeProfile()) {
        for (const auto& m : p->macros) maxId = std::max(maxId, m.id);
    }
    return maxId + 1;
}

void MainWindow::switchToProfile(std::uint64_t profileId) {
    if (profileId == config_.activeProfileId) return;

    // Stop engine before switching
    if (engine_.IsToolRunning()) {
        setToolRunning(false);
    }
    macroEngine_.StopAll();

    config_.activeProfileId = profileId;
    activeMacroId_ = 0;  // Reset active macro

    // Set active macro to first in new profile
    if (auto* p = activeProfile()) {
        if (!p->macros.empty()) activeMacroId_ = p->macros.front().id;
    }

    applyBindingsToEngine();
    applyMacrosToEngine();
    saveConfig();
    refreshAllPages();
}

// ── Config load/save ────────────────────────────────────────────────────────

void MainWindow::loadConfig() {
    config_ = configStore_.Load();

    // Ensure IDs are valid
    for (auto& profile : config_.profiles) {
        for (auto& b : profile.bindings) {
            if (b.id == 0) b.id = nextBindingIdInProfile();
        }
        for (auto& m : profile.macros) {
            if (m.id == 0) m.id = nextMacroIdInProfile();
        }
        // Ensure at least one macro per profile
        if (profile.macros.empty()) {
            MacroConfig defaultMacro;
            defaultMacro.id = 1;
            defaultMacro.name = L"\u9ed8\u8ba4\u5b8f";
            defaultMacro.enabled = true;
            profile.macros.push_back(defaultMacro);
        }
    }

    // Set active macro
    if (auto* p = activeProfile()) {
        if (activeMacroId_ == 0 || std::none_of(p->macros.begin(), p->macros.end(),
            [this](const MacroConfig& m) { return m.id == activeMacroId_; })) {
            activeMacroId_ = p->macros.front().id;
        }
    }

    refreshHookState();
}

void MainWindow::saveConfig() {
    std::wstring error;
    if (!configStore_.Save(config_, error)) {
        QMessageBox::warning(this, QString::fromWCharArray(L"\u4fdd\u5b58\u914d\u7f6e\u5931\u8d25"), QString::fromStdWString(error));
    }
}

void MainWindow::refreshAllPages() {
    FactionTheme::instance().setCurrent(QString::fromStdString(config_.faction));
    sidebar_->updateFactionIcon();

    auto* profile = activeProfile();
    if (!profile) return;

    QString hotkey = profile->controlHotkey.has_value()
        ? QString::fromStdWString(GetKeyDisplayName(*profile->controlHotkey))
        : QString();
    keyPage_->setBindings(profile->bindings);
    keyPage_->setRunning(engine_.IsToolRunning());
    keyPage_->setHotkeyText(hotkey.isEmpty() ? QString::fromWCharArray(L"\u672a\u8bbe\u7f6e") : hotkey);
    keyPage_->setBeepEnabled(config_.beepEnabled);
    keyPage_->setMixedMode(profile->mixedMode);
    keyPage_->setPauseKeys(profile->pauseKeys);
    keyPage_->updateThemeColors();
    refreshProfileCombo();
    refreshMacroPage();
    macroPage_->setBeepEnabled(config_.macroBeepEnabled);
    macroPage_->updateThemeColors();
    settingsPage_->setConfig(config_);
    settingsPage_->setHotkeyText(hotkey.isEmpty() ? QString::fromWCharArray(L"\u672a\u8bbe\u7f6e") : hotkey);
    settingsPage_->setConfigPath(QString::fromStdWString(configStore_.GetPath().wstring()));
    settingsPage_->setCloseAction(config_.closeAction);
    settingsPage_->setFaction(QString::fromStdString(config_.faction));
    settingsPage_->setCustomSoundPaths(QString(), QString());
}

void MainWindow::refreshProfileCombo() {
    std::vector<std::pair<std::uint64_t, QString>> profiles;
    for (const auto& p : config_.profiles) {
        profiles.emplace_back(p.id, QString::fromStdWString(p.name));
    }
    keyPage_->setProfiles(profiles, config_.activeProfileId);
}

void MainWindow::refreshMacroPage() {
    auto* profile = activeProfile();
    if (!profile) return;

    std::vector<std::pair<std::uint64_t, QString>> macroList;
    for (const auto& m : profile->macros) {
        macroList.emplace_back(m.id, QString::fromStdWString(m.name));
    }
    macroPage_->setMacroProfiles(macroList, activeMacroId_);

    if (auto* macro = findActiveMacro()) {
        macroPage_->setSteps(macro->steps);
        macroPage_->setMode(macro->triggerMode);
        macroPage_->setLoopCount(macro->loopCount);
        lastMacroRunning_ = macroEngine_.IsRunning(macro->id);
        QString hotkeyText = macro->triggerKey.has_value()
            ? QString::fromStdWString(GetKeyDisplayName(*macro->triggerKey))
            : QString::fromWCharArray(L"\u672a\u8bbe\u7f6e");
        macroPage_->setHotkeyText(hotkeyText);
    } else {
        macroPage_->setSteps({});
        macroPage_->setMode(MacroTriggerMode::Toggle);
        macroPage_->setLoopCount(0);
        macroPage_->setHotkeyText(QString::fromWCharArray(L"\u672a\u8bbe\u7f6e"));
    }
}

void MainWindow::applyBindingsToEngine() {
    auto* profile = activeProfile();
    if (!profile) return;
    engine_.UpdateBindings(profile->bindings);
    engine_.SetMixedMode(profile->mixedMode);
    engine_.SetPauseKeys(profile->pauseKeys);
    refreshHookState();
    updateStatusText();
}

void MainWindow::applyMacrosToEngine() {
    auto* profile = activeProfile();
    if (!profile) {
        macroEngine_.UpdateMacros({});
        return;
    }
    // 只把当前激活的宏传给引擎（不同宏可以设置相同触发键，同一时间只激活一个）
    auto* macro = findActiveMacro();
    if (macro) {
        macroEngine_.UpdateMacros({*macro});
    } else {
        macroEngine_.UpdateMacros({});
    }
}

MacroConfig* MainWindow::findActiveMacro() {
    auto* profile = activeProfile();
    if (!profile) return nullptr;
    for (auto& m : profile->macros) {
        if (m.id == activeMacroId_) return &m;
    }
    return nullptr;
}

// ── Engine control ──────────────────────────────────────────────────────────

void MainWindow::updateStatusText() {
    keyPage_->setRunning(engine_.IsToolRunning());
}

void MainWindow::updateHotkeyText() {
    auto* profile = activeProfile();
    QString text = captureRequest_.has_value() && captureRequest_->target == CaptureRequest::Target::MainHotkey
        ? QString::fromWCharArray(L"\u8bf7\u6309\u4e0b\u4e00\u4e2a\u952e...")
        : (profile && profile->controlHotkey.has_value()
            ? QString::fromStdWString(GetKeyDisplayName(*profile->controlHotkey))
            : QString::fromWCharArray(L"\u672a\u8bbe\u7f6e"));
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
    updateModeSwitchStyle();
    playToggleSound(running);
}

void MainWindow::playToggleSound(bool running, bool macroMode) const {
    if (macroMode) {
        if (!config_.macroBeepEnabled) return;
    } else if (!config_.beepEnabled) {
        return;
    }

    const auto soundPath = resolveToggleSoundPath(running, macroMode);
    if (soundPath.empty()) return;
    playSoundFile(soundPath,
        macroMode ? config_.macroSoundVolume : config_.soundVolume,
        macroMode ? L"autokey_macro_toggle_sound" : L"autokey_toggle_sound");
}

void MainWindow::playSoundFile(const std::filesystem::path& soundPath, int volume, const wchar_t* alias) const {
    if (soundPath.empty()) return;

    const std::wstring aliasName(alias);
    mciSendStringW((L"close " + aliasName).c_str(), nullptr, 0, nullptr);
    const std::wstring openCommand = L"open \"" + soundPath.wstring() + L"\" type mpegvideo alias " + aliasName;
    if (mciSendStringW(openCommand.c_str(), nullptr, 0, nullptr) != 0) return;

    const int mciVolume = std::clamp(volume, 0, 100) * 10;
    const std::wstring volumeCommand = L"setaudio " + aliasName + L" volume to " + std::to_wstring(mciVolume);
    mciSendStringW(volumeCommand.c_str(), nullptr, 0, nullptr);
    mciSendStringW((L"play " + aliasName + L" from 0").c_str(), nullptr, 0, nullptr);
}

std::filesystem::path MainWindow::resolveToggleSoundPath(bool running, bool macroMode) const {
    std::filesystem::path soundPath;
    // 优先使用自定义提示音
    const auto& customOn = macroMode ? config_.macroSoundOn : config_.customSoundOn;
    const auto& customOff = macroMode ? config_.macroSoundOff : config_.customSoundOff;
    if (running && !customOn.empty()) {
        soundPath = customOn;
        if (!std::filesystem::exists(soundPath)) soundPath.clear();
    } else if (!running && !customOff.empty()) {
        soundPath = customOff;
        if (!std::filesystem::exists(soundPath)) soundPath.clear();
    }
    // 回退到默认内置音频
    if (soundPath.empty()) {
        if (macroMode) {
            soundPath = resolveAudioPath(running ? L"\u5b8f\u5f00\u542f.mp3" : L"\u5b8f\u5173\u95ed.mp3");
        } else {
            soundPath = resolveAudioPath(running ? L"\u6309\u952e\u5f00\u542f.mp3" : L"\u6309\u952e\u5173\u95ed.mp3");
        }
    }
    return soundPath;
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
    auto* profile = activeProfile();
    if (!profile) return false;
    return std::any_of(profile->bindings.begin(), profile->bindings.end(), [&](const KeyBinding& b) {
        return b.enabled && b.mode == RepeatMode::Hold && b.key == key;
    });
}

bool MainWindow::IsHoldBindingEnabledForKeyRealtime(const KeyDescriptor& key) const {
    return IsHoldBindingEnabledForKey(key);
}

// ── Binding management ──────────────────────────────────────────────────────

void MainWindow::onAddBinding() { openEditDialog(std::nullopt); }

void MainWindow::onEditBinding(std::uint64_t id) { openEditDialog(id); }

void MainWindow::onDeleteBinding(std::uint64_t id) {
    auto* profile = activeProfile();
    if (!profile) return;
    profile->bindings.erase(
        std::remove_if(profile->bindings.begin(), profile->bindings.end(),
            [id](const KeyBinding& b) { return b.id == id; }),
        profile->bindings.end());
    keyPage_->setBindings(profile->bindings);
    applyBindingsToEngine();
    saveConfig();
    refreshAllPages();
}

void MainWindow::onEnableToggled(std::uint64_t id, bool enabled) {
    auto* profile = activeProfile();
    if (!profile) return;
    for (auto& b : profile->bindings) {
        if (b.id == id) { b.enabled = enabled; break; }
    }
    applyBindingsToEngine();
    saveConfig();
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
    keyPage_->setBeepEnabled(enabled);
    saveConfig();
}

void MainWindow::onMacroBeepToggled(bool enabled) {
    config_.macroBeepEnabled = enabled;
    macroPage_->setBeepEnabled(enabled);
    saveConfig();
}

void MainWindow::onMixedModeToggled(bool enabled) {
    auto* profile = activeProfile();
    if (!profile) return;
    profile->mixedMode = enabled;
    engine_.SetMixedMode(enabled);
    saveConfig();
}

// ── Pause Key Management ────────────────────────────────────────────────────

void MainWindow::onPauseKeyAdd() {
    auto* profile = activeProfile();
    if (!profile) return;

    QDialog dialog(this);
    dialog.setWindowTitle(QString::fromWCharArray(L"\u6dfb\u52a0\u6682\u505c\u6309\u952e"));
    dialog.setFixedSize(320, 140);
    dialog.setWindowFlags(dialog.windowFlags() & ~Qt::WindowContextHelpButtonHint);
    dialog.setStyleSheet("QDialog { background: #FFFFFF; }");

    auto* layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(24, 20, 24, 16);
    layout->setSpacing(12);

    auto* keyRow = new QHBoxLayout();
    auto* keyLabel = new QLabel(QString::fromWCharArray(L"\u6309\u952e:"), &dialog);
    keyLabel->setFont(Fluent::Typography::body());
    keyLabel->setFixedWidth(50);
    keyRow->addWidget(keyLabel);
    auto* keyEdit = new FluentLineEdit(&dialog);
    keyEdit->setReadOnly(true);
    keyEdit->setPlaceholderText(QString::fromWCharArray(L"\u70b9\u51fb\u5f55\u5236\u6309\u952e..."));
    keyRow->addWidget(keyEdit);
    layout->addLayout(keyRow);

    layout->addStretch();

    auto* btnRow = new QHBoxLayout();
    btnRow->addStretch();
    auto* cancelBtn = new FluentButton(FluentButton::Secondary, QString::fromWCharArray(L"\u53d6\u6d88"), &dialog);
    cancelBtn->setFixedWidth(80);
    auto* okBtn = new FluentButton(FluentButton::Primary, QString::fromWCharArray(L"\u786e\u5b9a"), &dialog);
    okBtn->setFixedWidth(80);
    btnRow->addWidget(cancelBtn);
    btnRow->addSpacing(8);
    btnRow->addWidget(okBtn);
    layout->addLayout(btnRow);

    KeyDescriptor capturedKey{};
    auto* prevEditKeyBox = editKeyBox_;
    editKeyBox_ = keyEdit;
    keyEdit->installEventFilter(this);

    auto captureConn = connect(this, &MainWindow::capturedKey, &dialog, [&](quint64 encodedKey) {
        KeyDescriptor k = DecodeKey(encodedKey);
        if (k.device == InputDevice::Mouse && (k.code == VK_LBUTTON || k.code == VK_RBUTTON)) {
            keyEdit->setText(QString::fromWCharArray(L"\u70b9\u51fb\u5f55\u5236\u6309\u952e..."));
            return;
        }
        capturedKey = k;
        keyEdit->setText(QString::fromStdWString(GetKeyDisplayName(capturedKey)));
        captureRequest_.reset();
        refreshHookState();
    });

    connect(cancelBtn, &QPushButton::clicked, &dialog, &QDialog::reject);
    connect(okBtn, &QPushButton::clicked, &dialog, [&]() {
        if (!capturedKey.IsValid()) {
            QMessageBox::warning(&dialog, QString::fromWCharArray(L"\u53c2\u6570\u65e0\u6548"),
                QString::fromWCharArray(L"\u8bf7\u5148\u5f55\u5236\u4e00\u4e2a\u6309\u952e\u3002"));
            return;
        }
        // 检查冲突：不能和启停快捷键相同
        if (profile->controlHotkey.has_value() && *profile->controlHotkey == capturedKey) {
            QMessageBox::warning(&dialog, QString::fromWCharArray(L"\u6309\u952e\u51b2\u7a81"),
                QString::fromWCharArray(L"\u8be5\u6309\u952e\u5df2\u88ab\u8bbe\u7f6e\u4e3a\u542f\u505c\u5feb\u6377\u952e\u3002"));
            return;
        }
        // 检查冲突：不能和已有的暂停键重复
        for (const auto& pk : profile->pauseKeys) {
            if (pk == capturedKey) {
                QMessageBox::warning(&dialog, QString::fromWCharArray(L"\u6309\u952e\u91cd\u590d"),
                    QString::fromWCharArray(L"\u8be5\u6309\u952e\u5df2\u5728\u6682\u505c\u952e\u5217\u8868\u4e2d\u3002"));
                return;
            }
        }
        // 检查冲突：不能和连发按键相同
        for (const auto& b : profile->bindings) {
            if (b.key == capturedKey) {
                QMessageBox::warning(&dialog, QString::fromWCharArray(L"\u6309\u952e\u51b2\u7a81"),
                    QString::fromWCharArray(L"\u8be5\u6309\u952e\u5df2\u88ab\u914d\u7f6e\u4e3a\u8fde\u53d1\u6309\u952e\u3002"));
                return;
            }
        }
        profile->pauseKeys.push_back(capturedKey);
        dialog.accept();
    });

    dialogOpen_ = true;
    dialog.exec();
    dialogOpen_ = false;
    disconnect(captureConn);
    editKeyBox_ = prevEditKeyBox;
    captureRequest_.reset();
    refreshHookState();

    applyBindingsToEngine();
    saveConfig();
    keyPage_->setPauseKeys(profile->pauseKeys);
}

void MainWindow::onPauseKeyDelete(int index) {
    auto* profile = activeProfile();
    if (!profile) return;
    if (index < 0 || index >= static_cast<int>(profile->pauseKeys.size())) return;
    profile->pauseKeys.erase(profile->pauseKeys.begin() + index);
    applyBindingsToEngine();
    saveConfig();
    keyPage_->setPauseKeys(profile->pauseKeys);
}

void MainWindow::onSoundSettingsClicked() {
    const bool macroMode = keyMacroStack_->currentWidget() == macroPage_;
    QDialog dialog(this);
    dialog.setWindowTitle(macroMode
        ? QString::fromWCharArray(L"\u5b8f\u97f3\u6548\u8bbe\u7f6e")
        : QString::fromWCharArray(L"\u6309\u952e\u97f3\u6548\u8bbe\u7f6e"));
    dialog.setFixedSize(430, 260);
    dialog.setWindowFlags(dialog.windowFlags() & ~Qt::WindowContextHelpButtonHint);
    const QString accentColor = FactionTheme::instance().primaryColor().name();
    dialog.setStyleSheet(QString(
        "QDialog { background: #FFFFFF; }"
        "QLabel { color: #202020; }"
        "QSlider::groove:horizontal { height: 4px; background: rgba(0,0,0,0.10); border-radius: 2px; }"
        "QSlider::handle:horizontal { width: 14px; height: 14px; margin: -5px 0; border-radius: 7px; background: %1; }"
        "QSlider::sub-page:horizontal { background: %1; border-radius: 2px; }"
    ).arg(accentColor));

    QString soundOnPath = QString::fromStdWString(macroMode ? config_.macroSoundOn : config_.customSoundOn);
    QString soundOffPath = QString::fromStdWString(macroMode ? config_.macroSoundOff : config_.customSoundOff);
    int soundVolume = std::clamp(macroMode ? config_.macroSoundVolume : config_.soundVolume, 0, 100);

    auto* layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(22, 18, 22, 18);
    layout->setSpacing(10);

    auto* titleRow = new QHBoxLayout();
    auto* titleIcon = new QLabel(QString(QChar(0xE189)), &dialog);
    titleIcon->setFont(QFont("Segoe MDL2 Assets", 18));
    titleIcon->setFixedWidth(28);
    titleRow->addWidget(titleIcon);
    auto* title = new QLabel(macroMode
        ? QString::fromWCharArray(L"\u5b8f\u97f3\u6548\u8bbe\u7f6e")
        : QString::fromWCharArray(L"\u6309\u952e\u97f3\u6548\u8bbe\u7f6e"), &dialog);
    title->setFont(Fluent::Typography::cardTitle());
    titleRow->addWidget(title);
    titleRow->addStretch();
    auto* closeBtn = new FluentButton(FluentButton::Icon, QString(QChar(0xE711)), &dialog);
    closeBtn->setFont(QFont("Segoe MDL2 Assets", 10));
    closeBtn->setFixedSize(28, 28);
    closeBtn->setToolTip(QString::fromWCharArray(L"\u5173\u95ed"));
    titleRow->addWidget(closeBtn);
    layout->addLayout(titleRow);

    auto createSoundRow = [&](const QString& labelText, QString* pathValue, bool running) {
        auto* row = new QHBoxLayout();
        row->setSpacing(8);

        auto* label = new QLabel(labelText, &dialog);
        label->setFont(Fluent::Typography::body());
        label->setFixedWidth(76);
        row->addWidget(label);

        auto* pathEdit = new FluentLineEdit(&dialog);
        pathEdit->setReadOnly(true);
        pathEdit->setPlaceholderText(QString::fromWCharArray(L"\u9ed8\u8ba4\u5185\u7f6e\u97f3\u6548"));
        pathEdit->setText(*pathValue);
        pathEdit->setCursor(Qt::PointingHandCursor);
        pathEdit->setToolTip(QString::fromWCharArray(L"\u70b9\u51fb\u9009\u62e9\u97f3\u9891"));
        row->addWidget(pathEdit, 1);

        auto* previewBtn = new FluentButton(FluentButton::Primary, QString(QChar(0xE768)), &dialog);
        previewBtn->setFont(QFont("Segoe MDL2 Assets", 11));
        previewBtn->setFixedSize(32, 30);
        previewBtn->setToolTip(QString::fromWCharArray(L"\u8bd5\u542c"));
        row->addWidget(previewBtn);

        auto* resetBtn = new FluentButton(FluentButton::Icon, QString(QChar(0xE74D)), &dialog);
        resetBtn->setFont(QFont("Segoe MDL2 Assets", 12));
        resetBtn->setFixedSize(30, 30);
        resetBtn->setToolTip(QString::fromWCharArray(L"\u6062\u590d\u9ed8\u8ba4"));
        row->addWidget(resetBtn);

        connect(pathEdit, &FluentLineEdit::clicked, &dialog, [&, pathValue, pathEdit, running]() {
            const QString caption = running
                ? (macroMode ? QString::fromWCharArray(L"\u9009\u62e9\u5b8f\u5f00\u542f\u97f3\u6548") : QString::fromWCharArray(L"\u9009\u62e9\u6309\u952e\u5f00\u542f\u97f3\u6548"))
                : (macroMode ? QString::fromWCharArray(L"\u9009\u62e9\u5b8f\u5173\u95ed\u97f3\u6548") : QString::fromWCharArray(L"\u9009\u62e9\u6309\u952e\u5173\u95ed\u97f3\u6548"));
            QString filePath = QFileDialog::getOpenFileName(&dialog,
                caption,
                QString(),
                QString::fromWCharArray(L"\u97f3\u9891\u6587\u4ef6 (*.mp3 *.wav *.wma *.mid *.midi)"));
            if (filePath.isEmpty()) return;
            if (!IsValidAudioFile(filePath)) {
                QMessageBox::warning(&dialog,
                    QString::fromWCharArray(L"\u683c\u5f0f\u4e0d\u652f\u6301"),
                    QString::fromWCharArray(L"\u8bf7\u9009\u62e9 mp3\u3001wav\u3001wma\u3001mid \u683c\u5f0f\u7684\u97f3\u9891\u6587\u4ef6\u3002"));
                return;
            }
            *pathValue = filePath;
            pathEdit->setText(filePath);
        });

        connect(previewBtn, &QPushButton::clicked, &dialog, [&, pathValue, running]() {
            std::filesystem::path previewPath;
            if (!pathValue->isEmpty()) previewPath = pathValue->toStdWString();
            if (previewPath.empty() || !std::filesystem::exists(previewPath)) {
                previewPath = resolveToggleSoundPath(running, macroMode);
            }
            playSoundFile(previewPath, soundVolume, L"autokey_preview_sound");
        });

        connect(resetBtn, &QPushButton::clicked, &dialog, [pathValue, pathEdit]() {
            pathValue->clear();
            pathEdit->clear();
        });

        layout->addLayout(row);
    };

    createSoundRow(QString::fromWCharArray(L"\u6309\u952e\u5f00\u542f"), &soundOnPath, true);
    createSoundRow(QString::fromWCharArray(L"\u6309\u952e\u5173\u95ed"), &soundOffPath, false);

    auto* volumeRow = new QHBoxLayout();
    volumeRow->setSpacing(8);
    auto* volumeLabel = new QLabel(QString::fromWCharArray(L"\u97f3\u91cf"), &dialog);
    volumeLabel->setFont(Fluent::Typography::body());
    volumeLabel->setFixedWidth(76);
    volumeRow->addWidget(volumeLabel);
    auto* volumeSlider = new QSlider(Qt::Horizontal, &dialog);
    volumeSlider->setRange(0, 100);
    volumeSlider->setValue(soundVolume);
    volumeRow->addWidget(volumeSlider, 1);
    auto* volumeValueLabel = new QLabel(QString::number(soundVolume) + "%", &dialog);
    volumeValueLabel->setFont(Fluent::Typography::caption());
    volumeValueLabel->setFixedWidth(42);
    volumeValueLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    volumeRow->addWidget(volumeValueLabel);
    layout->addLayout(volumeRow);

    auto* desc = new QLabel(QString::fromWCharArray(L"\u652f\u6301 mp3\u3001wav\u3001wma\u3001mid \u683c\u5f0f\uff1b\u6e05\u7a7a\u8def\u5f84\u540e\u4f7f\u7528\u5185\u7f6e\u9ed8\u8ba4\u97f3\u6548\u3002"), &dialog);
    desc->setFont(Fluent::Typography::caption());
    desc->setWordWrap(true);
    QPalette descPal = desc->palette();
    descPal.setColor(QPalette::WindowText, Fluent::Colors::textSecondary());
    desc->setPalette(descPal);
    layout->addWidget(desc);

    layout->addStretch();

    auto* buttonRow = new QHBoxLayout();
    buttonRow->addStretch();
    auto* cancelBtn = new FluentButton(FluentButton::Secondary, QString::fromWCharArray(L"\u53d6\u6d88"), &dialog);
    cancelBtn->setFixedWidth(82);
    auto* saveBtn = new FluentButton(FluentButton::Primary, QString::fromWCharArray(L"\u4fdd\u5b58"), &dialog);
    saveBtn->setFixedWidth(94);
    buttonRow->addWidget(cancelBtn);
    buttonRow->addSpacing(8);
    buttonRow->addWidget(saveBtn);
    layout->addLayout(buttonRow);

    connect(volumeSlider, &QSlider::valueChanged, &dialog, [&](int value) {
        soundVolume = value;
        volumeValueLabel->setText(QString::number(value) + "%");
    });
    connect(closeBtn, &QPushButton::clicked, &dialog, &QDialog::reject);
    connect(cancelBtn, &QPushButton::clicked, &dialog, &QDialog::reject);
    connect(saveBtn, &QPushButton::clicked, &dialog, &QDialog::accept);

    dialogOpen_ = true;
    const int result = dialog.exec();
    dialogOpen_ = false;
    mciSendStringW(L"close autokey_preview_sound", nullptr, 0, nullptr);

    if (result != QDialog::Accepted) return;

    if (macroMode) {
        config_.macroSoundOn = soundOnPath.toStdWString();
        config_.macroSoundOff = soundOffPath.toStdWString();
        config_.macroSoundVolume = soundVolume;
    } else {
        config_.customSoundOn = soundOnPath.toStdWString();
        config_.customSoundOff = soundOffPath.toStdWString();
        config_.soundVolume = soundVolume;
    }
    saveConfig();
}

// ── Profile Management ──────────────────────────────────────────────────────

void MainWindow::onProfileSwitched(quint64 profileId) {
    if (profileId == config_.activeProfileId) return;
    switchToProfile(profileId);
}

void MainWindow::onProfileAdd() {
    dialogOpen_ = true;
    bool ok = false;
    QString name = QInputDialog::getText(this,
        QString::fromWCharArray(L"\u65b0\u5efa\u914d\u7f6e"),
        QString::fromWCharArray(L"\u8bf7\u8f93\u5165\u914d\u7f6e\u540d\u79f0:"),
        QLineEdit::Normal, QString(), &ok);
    dialogOpen_ = false;
    if (!ok || name.trimmed().isEmpty()) return;

    ProfileConfig newProfile;
    newProfile.id = nextProfileId();
    newProfile.name = name.trimmed().toStdWString();
    // Add a default macro
    MacroConfig defaultMacro;
    defaultMacro.id = 1;
    defaultMacro.name = L"\u9ed8\u8ba4\u5b8f";
    defaultMacro.enabled = true;
    newProfile.macros.push_back(defaultMacro);

    config_.profiles.push_back(newProfile);
    switchToProfile(newProfile.id);
}

void MainWindow::onProfileRename() {
    auto* profile = activeProfile();
    if (!profile) return;

    dialogOpen_ = true;
    bool ok = false;
    QString name = QInputDialog::getText(this,
        QString::fromWCharArray(L"\u91cd\u547d\u540d\u914d\u7f6e"),
        QString::fromWCharArray(L"\u65b0\u540d\u79f0:"),
        QLineEdit::Normal, QString::fromStdWString(profile->name), &ok);
    dialogOpen_ = false;
    if (!ok || name.trimmed().isEmpty()) return;

    profile->name = name.trimmed().toStdWString();
    saveConfig();
    refreshProfileCombo();
}

void MainWindow::onProfileDelete() {
    if (config_.profiles.size() <= 1) {
        QMessageBox::information(this,
            QString::fromWCharArray(L"\u65e0\u6cd5\u5220\u9664"),
            QString::fromWCharArray(L"\u81f3\u5c11\u9700\u8981\u4fdd\u7559\u4e00\u4e2a\u914d\u7f6e\u65b9\u6848\u3002"));
        return;
    }

    auto* profile = activeProfile();
    if (!profile) return;

    dialogOpen_ = true;
    auto result = QMessageBox::question(this,
        QString::fromWCharArray(L"\u5220\u9664\u914d\u7f6e"),
        QString::fromWCharArray(L"\u786e\u5b9a\u8981\u5220\u9664\u914d\u7f6e\u201c") +
            QString::fromStdWString(profile->name) +
            QString::fromWCharArray(L"\u201d\u5417\uff1f\u6b64\u64cd\u4f5c\u4e0d\u53ef\u64a4\u9500\u3002"),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    dialogOpen_ = false;
    if (result != QMessageBox::Yes) return;

    std::uint64_t deletedId = profile->id;
    config_.profiles.erase(
        std::remove_if(config_.profiles.begin(), config_.profiles.end(),
            [deletedId](const ProfileConfig& p) { return p.id == deletedId; }),
        config_.profiles.end());

    // Switch to first remaining profile
    config_.activeProfileId = config_.profiles.front().id;
    activeMacroId_ = 0;
    if (auto* p = activeProfile()) {
        if (!p->macros.empty()) activeMacroId_ = p->macros.front().id;
    }

    applyBindingsToEngine();
    applyMacrosToEngine();
    saveConfig();
    refreshAllPages();
}

// ── Macro Management ────────────────────────────────────────────────────────

void MainWindow::onMacroSwitched(quint64 macroId) {
    if (macroId == activeMacroId_) return;
    activeMacroId_ = macroId;
    applyMacrosToEngine();
    refreshMacroPage();
}

void MainWindow::onMacroAdd() {
    auto* profile = activeProfile();
    if (!profile) return;

    dialogOpen_ = true;
    bool ok = false;
    QString name = QInputDialog::getText(this,
        QString::fromWCharArray(L"\u65b0\u5efa\u5b8f"),
        QString::fromWCharArray(L"\u8bf7\u8f93\u5165\u5b8f\u540d\u79f0:"),
        QLineEdit::Normal, QString(), &ok);
    dialogOpen_ = false;
    if (!ok || name.trimmed().isEmpty()) return;

    MacroConfig macro;
    macro.id = nextMacroIdInProfile();
    macro.name = name.trimmed().toStdWString();
    macro.enabled = true;
    profile->macros.push_back(macro);
    activeMacroId_ = macro.id;
    applyMacrosToEngine();
    saveConfig();
    refreshMacroPage();
}

void MainWindow::onMacroRename() {
    auto* macro = findActiveMacro();
    if (!macro) return;

    dialogOpen_ = true;
    bool ok = false;
    QString name = QInputDialog::getText(this,
        QString::fromWCharArray(L"\u91cd\u547d\u540d\u5b8f"),
        QString::fromWCharArray(L"\u65b0\u540d\u79f0:"),
        QLineEdit::Normal, QString::fromStdWString(macro->name), &ok);
    dialogOpen_ = false;
    if (!ok || name.trimmed().isEmpty()) return;

    macro->name = name.trimmed().toStdWString();
    saveConfig();
    refreshMacroPage();
}

void MainWindow::onMacroDelete() {
    auto* profile = activeProfile();
    if (!profile) return;

    if (profile->macros.size() <= 1) {
        QMessageBox::information(this,
            QString::fromWCharArray(L"\u65e0\u6cd5\u5220\u9664"),
            QString::fromWCharArray(L"\u81f3\u5c11\u9700\u8981\u4fdd\u7559\u4e00\u4e2a\u5b8f\u914d\u7f6e\u3002"));
        return;
    }
    auto* macro = findActiveMacro();
    if (!macro) return;

    dialogOpen_ = true;
    auto result = QMessageBox::question(this,
        QString::fromWCharArray(L"\u5220\u9664\u5b8f"),
        QString::fromWCharArray(L"\u786e\u5b9a\u8981\u5220\u9664\u5b8f\u201c") +
            QString::fromStdWString(macro->name) +
            QString::fromWCharArray(L"\u201d\u5417\uff1f"),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    dialogOpen_ = false;
    if (result != QMessageBox::Yes) return;

    profile->macros.erase(
        std::remove_if(profile->macros.begin(), profile->macros.end(),
            [this](const MacroConfig& m) { return m.id == activeMacroId_; }),
        profile->macros.end());
    activeMacroId_ = profile->macros.empty() ? 0 : profile->macros.front().id;
    applyMacrosToEngine();
    saveConfig();
    refreshMacroPage();
}

void MainWindow::onMacroAddStep() {
    auto* macro = findActiveMacro();
    if (!macro) return;

    QDialog dialog(this);
    dialog.setWindowTitle(QString::fromWCharArray(L"\u6dfb\u52a0\u6b65\u9aa4"));
    dialog.setFixedSize(360, 180);
    dialog.setWindowFlags(dialog.windowFlags() & ~Qt::WindowContextHelpButtonHint);
    dialog.setStyleSheet("QDialog { background: #FFFFFF; }");

    auto* layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(24, 20, 24, 16);
    layout->setSpacing(12);

    auto* keyRow = new QHBoxLayout();
    auto* keyLabel = new QLabel(QString::fromWCharArray(L"\u6309\u952e:"), &dialog);
    keyLabel->setFont(Fluent::Typography::body());
    keyLabel->setFixedWidth(70);
    keyRow->addWidget(keyLabel);
    auto* keyEdit = new FluentLineEdit(&dialog);
    keyEdit->setReadOnly(true);
    keyEdit->setPlaceholderText(QString::fromWCharArray(L"\u70b9\u51fb\u5f55\u5236\u6309\u952e..."));
    keyRow->addWidget(keyEdit);
    layout->addLayout(keyRow);

    auto* delayRow = new QHBoxLayout();
    auto* delayLabel = new QLabel(QString::fromWCharArray(L"\u95f4\u9694(ms):"), &dialog);
    delayLabel->setFont(Fluent::Typography::body());
    delayLabel->setFixedWidth(70);
    delayRow->addWidget(delayLabel);
    auto* delayEdit = new FluentLineEdit(&dialog);
    delayEdit->setText("50");
    delayRow->addWidget(delayEdit);
    layout->addLayout(delayRow);

    layout->addStretch();

    auto* btnRow = new QHBoxLayout();
    btnRow->addStretch();
    auto* cancelBtn = new FluentButton(FluentButton::Secondary, QString::fromWCharArray(L"\u53d6\u6d88"), &dialog);
    cancelBtn->setFixedWidth(80);
    auto* okBtn = new FluentButton(FluentButton::Primary, QString::fromWCharArray(L"\u786e\u5b9a"), &dialog);
    okBtn->setFixedWidth(80);
    btnRow->addWidget(cancelBtn);
    btnRow->addSpacing(8);
    btnRow->addWidget(okBtn);
    layout->addLayout(btnRow);

    KeyDescriptor capturedKey{};
    auto* prevEditKeyBox = editKeyBox_;
    editKeyBox_ = keyEdit;
    keyEdit->installEventFilter(this);

    auto captureConn = connect(this, &MainWindow::capturedKey, &dialog, [&](quint64 encodedKey) {
        KeyDescriptor k = DecodeKey(encodedKey);
        if (k.device == InputDevice::Mouse && (k.code == VK_LBUTTON || k.code == VK_RBUTTON)) {
            keyEdit->setText(QString::fromWCharArray(L"\u70b9\u51fb\u5f55\u5236\u6309\u952e..."));
            return;
        }
        capturedKey = k;
        keyEdit->setText(QString::fromStdWString(GetKeyDisplayName(capturedKey)));
        captureRequest_.reset();
        refreshHookState();
    });

    connect(cancelBtn, &QPushButton::clicked, &dialog, &QDialog::reject);
    connect(okBtn, &QPushButton::clicked, &dialog, [&]() {
        if (!capturedKey.IsValid()) {
            QMessageBox::warning(&dialog, QString::fromWCharArray(L"\u53c2\u6570\u65e0\u6548"),
                QString::fromWCharArray(L"\u8bf7\u5148\u5f55\u5236\u4e00\u4e2a\u6309\u952e\u3002"));
            return;
        }
        // Check conflict with macro's trigger key
        if (macro->triggerKey.has_value() && *macro->triggerKey == capturedKey) {
            QMessageBox::warning(&dialog, QString::fromWCharArray(L"\u6309\u952e\u51b2\u7a81"),
                QString::fromWCharArray(L"\u8be5\u6309\u952e\u5df2\u88ab\u8bbe\u7f6e\u4e3a\u5f53\u524d\u5b8f\u7684\u542f\u505c\u5feb\u6377\u952e\u3002"));
            return;
        }
        bool ok = false;
        int delay = delayEdit->text().toInt(&ok);
        if (!ok || delay <= 0) delay = 50;
        macro->steps.push_back({capturedKey, delay});
        dialog.accept();
    });

    dialogOpen_ = true;
    dialog.exec();
    dialogOpen_ = false;
    disconnect(captureConn);
    editKeyBox_ = prevEditKeyBox;
    captureRequest_.reset();
    refreshHookState();

    applyMacrosToEngine();
    saveConfig();
    refreshMacroPage();
}

void MainWindow::onMacroEditStep(int stepIndex) {
    auto* macro = findActiveMacro();
    if (!macro || stepIndex < 0 || stepIndex >= static_cast<int>(macro->steps.size())) return;
    auto& step = macro->steps[stepIndex];

    QDialog dialog(this);
    dialog.setWindowTitle(QString::fromWCharArray(L"\u7f16\u8f91\u6b65\u9aa4"));
    dialog.setFixedSize(360, 180);
    dialog.setWindowFlags(dialog.windowFlags() & ~Qt::WindowContextHelpButtonHint);
    dialog.setStyleSheet("QDialog { background: #FFFFFF; }");

    auto* layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(24, 20, 24, 16);
    layout->setSpacing(12);

    auto* keyRow = new QHBoxLayout();
    auto* keyLabel = new QLabel(QString::fromWCharArray(L"\u6309\u952e:"), &dialog);
    keyLabel->setFont(Fluent::Typography::body());
    keyLabel->setFixedWidth(70);
    keyRow->addWidget(keyLabel);
    auto* keyEdit = new FluentLineEdit(&dialog);
    keyEdit->setReadOnly(true);
    keyEdit->setText(QString::fromStdWString(GetKeyDisplayName(step.key)));
    keyRow->addWidget(keyEdit);
    layout->addLayout(keyRow);

    auto* delayRow = new QHBoxLayout();
    auto* delayLabel = new QLabel(QString::fromWCharArray(L"\u95f4\u9694(ms):"), &dialog);
    delayLabel->setFont(Fluent::Typography::body());
    delayLabel->setFixedWidth(70);
    delayRow->addWidget(delayLabel);
    auto* delayEdit = new FluentLineEdit(&dialog);
    delayEdit->setText(QString::number(step.delayAfterMs));
    delayRow->addWidget(delayEdit);
    layout->addLayout(delayRow);

    layout->addStretch();

    auto* btnRow = new QHBoxLayout();
    btnRow->addStretch();
    auto* cancelBtn = new FluentButton(FluentButton::Secondary, QString::fromWCharArray(L"\u53d6\u6d88"), &dialog);
    cancelBtn->setFixedWidth(80);
    auto* okBtn = new FluentButton(FluentButton::Primary, QString::fromWCharArray(L"\u786e\u5b9a"), &dialog);
    okBtn->setFixedWidth(80);
    btnRow->addWidget(cancelBtn);
    btnRow->addSpacing(8);
    btnRow->addWidget(okBtn);
    layout->addLayout(btnRow);

    KeyDescriptor capturedKey = step.key;
    auto* prevEditKeyBox = editKeyBox_;
    editKeyBox_ = keyEdit;
    keyEdit->installEventFilter(this);

    auto captureConn = connect(this, &MainWindow::capturedKey, &dialog, [&](quint64 encodedKey) {
        KeyDescriptor k = DecodeKey(encodedKey);
        if (k.device == InputDevice::Mouse && (k.code == VK_LBUTTON || k.code == VK_RBUTTON)) {
            keyEdit->setText(QString::fromStdWString(GetKeyDisplayName(capturedKey)));
            return;
        }
        capturedKey = k;
        keyEdit->setText(QString::fromStdWString(GetKeyDisplayName(capturedKey)));
        captureRequest_.reset();
        refreshHookState();
    });

    connect(cancelBtn, &QPushButton::clicked, &dialog, &QDialog::reject);
    connect(okBtn, &QPushButton::clicked, &dialog, [&]() {
        if (!capturedKey.IsValid()) return;
        // Check conflict with macro's trigger key
        if (macro->triggerKey.has_value() && *macro->triggerKey == capturedKey) {
            QMessageBox::warning(&dialog, QString::fromWCharArray(L"\u6309\u952e\u51b2\u7a81"),
                QString::fromWCharArray(L"\u8be5\u6309\u952e\u5df2\u88ab\u8bbe\u7f6e\u4e3a\u5f53\u524d\u5b8f\u7684\u542f\u505c\u5feb\u6377\u952e\u3002"));
            return;
        }
        bool ok = false;
        int delay = delayEdit->text().toInt(&ok);
        if (!ok || delay <= 0) delay = 50;
        step.key = capturedKey;
        step.delayAfterMs = delay;
        dialog.accept();
    });

    dialogOpen_ = true;
    dialog.exec();
    dialogOpen_ = false;
    disconnect(captureConn);
    editKeyBox_ = prevEditKeyBox;
    captureRequest_.reset();
    refreshHookState();

    applyMacrosToEngine();
    saveConfig();
    refreshMacroPage();
}

void MainWindow::onMacroDeleteStep(int stepIndex) {
    auto* macro = findActiveMacro();
    if (!macro || stepIndex < 0 || stepIndex >= static_cast<int>(macro->steps.size())) return;
    macro->steps.erase(macro->steps.begin() + stepIndex);
    applyMacrosToEngine();
    saveConfig();
    refreshMacroPage();
}

void MainWindow::onMacroMoveStep(int fromIndex, int toIndex) {
    auto* macro = findActiveMacro();
    if (!macro) return;
    int count = static_cast<int>(macro->steps.size());
    if (fromIndex < 0 || fromIndex >= count || toIndex < 0 || toIndex >= count) return;
    if (fromIndex == toIndex) return;
    // 移动步骤：从原位置取出，插入到目标位置
    MacroStep step = macro->steps[fromIndex];
    macro->steps.erase(macro->steps.begin() + fromIndex);
    macro->steps.insert(macro->steps.begin() + toIndex, step);
    applyMacrosToEngine();
    saveConfig();
    refreshMacroPage();
}

void MainWindow::onMacroStartStopClicked() {
    auto* macro = findActiveMacro();
    if (!macro) return;

    bool isRunning = macroEngine_.IsRunning(macro->id);
    if (isRunning) {
        macroEngine_.StopAll();
    } else {
        if (macro->triggerKey.has_value()) {
            macroEngine_.OnTriggerDown(*macro->triggerKey);
        }
    }
    macroPage_->setRunning(!isRunning);
    updateModeSwitchStyle();
}

void MainWindow::onMacroHotkeyEditClicked() {
    captureRequest_ = CaptureRequest{CaptureRequest::Target::MacroHotkey};
    refreshHookState();
    { std::lock_guard lock(hookStateMutex_); hookCaptureStartedTick_ = GetTickCount64(); }
    macroPage_->setHotkeyText(QString::fromWCharArray(L"\u8bf7\u6309\u4e0b\u4e00\u4e2a\u952e..."));
}

void MainWindow::onMacroModeChanged(int modeIndex) {
    auto* macro = findActiveMacro();
    if (!macro) return;
    macro->triggerMode = modeIndex == 1 ? MacroTriggerMode::Hold : MacroTriggerMode::Toggle;
    applyMacrosToEngine();
    saveConfig();
}

void MainWindow::onMacroLoopCountChanged(int count) {
    auto* macro = findActiveMacro();
    if (!macro) return;
    macro->loopCount = count;
    applyMacrosToEngine();
    saveConfig();
}

// ── Edit Dialog ─────────────────────────────────────────────────────────────

void MainWindow::openEditDialog(std::optional<std::uint64_t> bindingId) {
    if (editDialog_) return;
    auto* profile = activeProfile();
    if (!profile) return;

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
            const auto& b = profile->bindings[*idx];
            currentEditKey_ = b.key;
            editModeCombo_->setCurrentIndex(b.mode == RepeatMode::Hold ? 1 : 0);
            editDelayEdit_->setText(QString::number(b.delayMs));
            editEnabledSwitch_->setChecked(b.enabled);
        }
    } else {
        editModeCombo_->setCurrentIndex(0);
        editDelayEdit_->setText(QString::number(profile->defaultDelayMs));
        editEnabledSwitch_->setChecked(true);
    }

    editKeyBox_->setText(currentEditKey_.IsValid()
        ? QString::fromStdWString(GetKeyDisplayName(currentEditKey_)) : QString());
    editKeyBox_->installEventFilter(this);

    connect(okBtn, &QPushButton::clicked, dialog, [this, dialog, profile]() {
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
        if (profile->controlHotkey.has_value() && *profile->controlHotkey == currentEditKey_) {
            QMessageBox::warning(dialog, QString::fromWCharArray(L"\u6309\u952e\u51b2\u7a81"),
                QString::fromWCharArray(L"\u8be5\u6309\u952e\u5df2\u88ab\u8bbe\u7f6e\u4e3a\u542f\u505c\u5feb\u6377\u952e\u3002"));
            return;
        }
        for (const auto& b : profile->bindings) {
            if (b.key == currentEditKey_) {
                if (editingExisting_ && b.id == editBindingId_) continue;
                QMessageBox::warning(dialog, QString::fromWCharArray(L"\u6309\u952e\u91cd\u590d"),
                    QString::fromWCharArray(L"\u8be5\u6309\u952e\u5df2\u88ab\u914d\u7f6e\u3002"));
                return;
            }
        }
        if (editingExisting_) {
            if (auto idx = findBindingIndex(editBindingId_)) {
                auto& b = profile->bindings[*idx];
                b.key = currentEditKey_;
                b.mode = editModeCombo_->currentIndex() == 1 ? RepeatMode::Hold : RepeatMode::Toggle;
                b.delayMs = delay;
                b.enabled = editEnabledSwitch_->isChecked();
            }
        } else {
            KeyBinding b;
            b.id = nextBindingIdInProfile();
            b.key = currentEditKey_;
            b.mode = editModeCombo_->currentIndex() == 1 ? RepeatMode::Hold : RepeatMode::Toggle;
            b.delayMs = delay;
            b.enabled = editEnabledSwitch_->isChecked();
            profile->bindings.push_back(b);
        }
        keyPage_->setBindings(profile->bindings);
        applyBindingsToEngine();
        saveConfig();
        refreshAllPages();
        dialog->accept();
    });
    connect(cancelBtn, &QPushButton::clicked, dialog, &QDialog::reject);

    dialogOpen_ = true;
    dialog->exec();
    dialogOpen_ = false;
    captureRequest_.reset();
    refreshHookState();
    updateHotkeyText();
    editDialog_ = nullptr;
    editKeyBox_ = nullptr;
}

std::optional<std::size_t> MainWindow::findBindingIndex(std::uint64_t id) const {
    auto* profile = activeProfile();
    if (!profile) return std::nullopt;
    for (std::size_t i = 0; i < profile->bindings.size(); ++i)
        if (profile->bindings[i].id == id) return i;
    return std::nullopt;
}

// ── Captured key handling ───────────────────────────────────────────────────

void MainWindow::handleCapturedKey(quint64 encodedKey) { onCapturedKeyInternal(DecodeKey(encodedKey)); }

void MainWindow::onCapturedKeyInternal(const KeyDescriptor& key) {
    if (!captureRequest_.has_value()) return;
    auto* profile = activeProfile();
    if (!profile) return;

    if (captureRequest_->target == CaptureRequest::Target::MainHotkey) {
        captureRequest_.reset();
        refreshHookState();
        if (key.device == InputDevice::Mouse && (key.code == VK_LBUTTON || key.code == VK_RBUTTON)) {
            QMessageBox::warning(this, QString::fromWCharArray(L"\u6309\u952e\u65e0\u6548"),
                QString::fromWCharArray(L"\u542f\u505c\u5feb\u6377\u952e\u4e0d\u80fd\u8bbe\u7f6e\u4e3a\u9f20\u6807\u5de6\u952e\u6216\u53f3\u952e\u3002"));
        } else {
            // Check conflict with bindings in this profile
            bool conflict = std::any_of(profile->bindings.begin(), profile->bindings.end(),
                [&](const KeyBinding& b) { return b.key == key; });
            if (conflict) {
                QMessageBox::warning(this, QString::fromWCharArray(L"\u6309\u952e\u51b2\u7a81"),
                    QString::fromWCharArray(L"\u8be5\u6309\u952e\u5df2\u88ab\u914d\u7f6e\u4e3a\u8fde\u53d1/\u6309\u538b\u6309\u952e\u3002"));
            } else {
                profile->controlHotkey = key;
                saveConfig();
                refreshHookState();
            }
        }
        updateHotkeyText();
        refreshAllPages();
    } else if (captureRequest_->target == CaptureRequest::Target::MacroHotkey) {
        captureRequest_.reset();
        refreshHookState();
        if (key.device == InputDevice::Mouse && (key.code == VK_LBUTTON || key.code == VK_RBUTTON)) {
            QMessageBox::warning(this, QString::fromWCharArray(L"\u6309\u952e\u65e0\u6548"),
                QString::fromWCharArray(L"\u542f\u505c\u5feb\u6377\u952e\u4e0d\u80fd\u8bbe\u7f6e\u4e3a\u9f20\u6807\u5de6\u952e\u6216\u53f3\u952e\u3002"));
        } else {
            auto* activeMacro = findActiveMacro();
            if (activeMacro) {
                // Check conflict with this macro's own step keys
                bool stepConflict = std::any_of(activeMacro->steps.begin(), activeMacro->steps.end(),
                    [&](const MacroStep& s) { return s.key == key; });
                if (stepConflict) {
                    QMessageBox::warning(this, QString::fromWCharArray(L"\u6309\u952e\u51b2\u7a81"),
                        QString::fromWCharArray(L"\u8be5\u6309\u952e\u5df2\u88ab\u5f53\u524d\u5b8f\u7684\u6b65\u9aa4\u4e2d\u4f7f\u7528\u3002"));
                } else {
                    activeMacro->triggerKey = key;
                    applyMacrosToEngine();
                    saveConfig();
                }
            }
        }
        refreshMacroPage();
    } else {
        currentEditKey_ = key;
        if (editKeyBox_) editKeyBox_->setText(QString::fromStdWString(GetKeyDisplayName(key)));
        captureRequest_.reset();
        refreshHookState();
    }
}

// ── Close event ─────────────────────────────────────────────────────────────

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

    dialogOpen_ = true;
    if (dialog.exec() == QDialog::Rejected) {
        dialogOpen_ = false;
        event->ignore();
        return;
    }
    dialogOpen_ = false;

    if (rememberCheck->isChecked()) { config_.closeAction = chosenExit ? CloseAction::Exit : CloseAction::Minimize; saveConfig(); }
    if (chosenExit) { trayIcon_->hide(); event->accept(); } else { hide(); event->ignore(); }
}

// ── Event filter ────────────────────────────────────────────────────────────

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

// ── Hook setup ──────────────────────────────────────────────────────────────

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

void MainWindow::refreshHookState() {
    std::lock_guard lock(hookStateMutex_);
    auto* profile = activeProfile();
    hookControlHotkey_ = profile ? profile->controlHotkey : std::nullopt;
    hookCaptureActive_ = captureRequest_.has_value();
    if (!hookCaptureActive_) hookCaptureStartedTick_ = 0;
}

// ── Utility ─────────────────────────────────────────────────────────────────

std::wstring MainWindow::getExeDir() const {
    std::wstring path(MAX_PATH, L'\0');
    DWORD length = GetModuleFileNameW(nullptr, path.data(), (DWORD)path.size());
    path.resize(length);
    PathRemoveFileSpecW(path.data());
    path.resize(wcslen(path.c_str()));
    return path;
}

std::filesystem::path MainWindow::getDefaultConfigPath() const {
    return std::filesystem::path(exeDir_) / L"config.json";
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
    const int activeMode = g_mainWindow->activeMode_.load();
    std::optional<KeyDescriptor> controlHotkey;
    bool captureActive = false;
    { std::lock_guard lock(g_mainWindow->hookStateMutex_); controlHotkey = g_mainWindow->hookControlHotkey_; captureActive = g_mainWindow->hookCaptureActive_; }

    if (captureActive && pressed) {
        auto enc = EncodeKey(key);
        QMetaObject::invokeMethod(g_mainWindow, [w = g_mainWindow, enc] { emit w->capturedKey(enc); }, Qt::QueuedConnection);
        return 1;
    }

    // 弹窗打开时，屏蔽启停快捷键和宏触发（但不屏蔽按键录制）
    if (g_mainWindow->dialogOpen_.load()) return CallNextHookEx(nullptr, code, wParam, lParam);

    // 连发模式（activeMode == 0）：响应连发快捷键和物理按键
    if (activeMode == 0) {
        const bool isCtrl = controlHotkey.has_value() && *controlHotkey == key;
        if (isCtrl) {
            if (pressed && !g_mainWindow->controlHotkeyPressed_.exchange(true)) {
                bool nr = !toolRunning;
                if (g_mainWindow->setToolRunningFast(nr))
                    QMetaObject::invokeMethod(g_mainWindow, [w = g_mainWindow, nr] { w->handleToolStateChanged(nr); }, Qt::QueuedConnection);
            } else if (released) g_mainWindow->controlHotkeyPressed_ = false;
        }
        if (toolRunning) { if (pressed) g_mainWindow->engine_.OnPhysicalDown(key); else if (released) g_mainWindow->engine_.OnPhysicalUp(key); }
    }

    // 宏模式（activeMode == 1）：响应宏触发键
    if (activeMode == 1) {
        if (pressed) g_mainWindow->macroEngine_.OnTriggerDown(key);
        else if (released) g_mainWindow->macroEngine_.OnTriggerUp(key);
    }

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
    const int activeMode = g_mainWindow->activeMode_.load();
    std::optional<KeyDescriptor> controlHotkey;
    bool captureActive = false;
    ULONGLONG captureStartedTick = 0;
    { std::lock_guard lock(g_mainWindow->hookStateMutex_); controlHotkey = g_mainWindow->hookControlHotkey_; captureActive = g_mainWindow->hookCaptureActive_; captureStartedTick = g_mainWindow->hookCaptureStartedTick_; }

    if (captureActive && pressed) {
        if (GetTickCount64() - captureStartedTick < 150) return CallNextHookEx(nullptr, code, wParam, lParam);
        auto enc = EncodeKey(key);
        QMetaObject::invokeMethod(g_mainWindow, [w = g_mainWindow, enc] { emit w->capturedKey(enc); }, Qt::QueuedConnection);
        return 1;
    }

    // 弹窗打开时，屏蔽启停快捷键和宏触发
    if (g_mainWindow->dialogOpen_.load()) return CallNextHookEx(nullptr, code, wParam, lParam);

    // 连发模式（activeMode == 0）：响应连发快捷键和物理按键
    if (activeMode == 0) {
        const bool isCtrl = controlHotkey.has_value() && *controlHotkey == key;
        if (isCtrl) {
            if (pressed && !g_mainWindow->controlHotkeyPressed_.exchange(true)) {
                bool nr = !toolRunning;
                if (g_mainWindow->setToolRunningFast(nr))
                    QMetaObject::invokeMethod(g_mainWindow, [w = g_mainWindow, nr] { w->handleToolStateChanged(nr); }, Qt::QueuedConnection);
            } else if (released) g_mainWindow->controlHotkeyPressed_ = false;
        }
        if (toolRunning) { if (pressed) g_mainWindow->engine_.OnPhysicalDown(key); else if (released) g_mainWindow->engine_.OnPhysicalUp(key); }
    }

    // 宏模式（activeMode == 1）：响应宏触发键
    if (activeMode == 1) {
        if (pressed) g_mainWindow->macroEngine_.OnTriggerDown(key);
        else if (released) g_mainWindow->macroEngine_.OnTriggerUp(key);
    }

    return CallNextHookEx(nullptr, code, wParam, lParam);
}
