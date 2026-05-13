#include "MainWindow.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QCloseEvent>
#include <QDialog>
#include <QDialogButtonBox>
#include <QApplication>
#include <QMetaObject>
#include <QClipboard>
#include <QGroupBox>
#include <QDesktopServices>
#include <QUrl>
#include <QRadioButton>
#include <QButtonGroup>
#include <QTabWidget>
#include <QFileDialog>
#include <shlwapi.h>
#include <ShlObj.h>
#include <mmsystem.h>
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
    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP:
        return {InputDevice::Mouse, VK_LBUTTON};
    case WM_RBUTTONDOWN:
    case WM_RBUTTONUP:
        return {InputDevice::Mouse, VK_RBUTTON};
    case WM_MBUTTONDOWN:
    case WM_MBUTTONUP:
        return {InputDevice::Mouse, VK_MBUTTON};
    case WM_XBUTTONDOWN:
    case WM_XBUTTONUP:
        return {InputDevice::Mouse, static_cast<std::uint32_t>(HIWORD(info.mouseData) == XBUTTON1 ? VK_XBUTTON1 : VK_XBUTTON2)};
    default:
        return {};
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
      configStore_(getConfigPath()),
      engine_(driver_) {
    g_mainWindow = this;
}

MainWindow::~MainWindow() {
    if (hookThreadId_ != 0) {
        PostThreadMessageW(hookThreadId_, WM_QUIT, 0, 0);
    }
    if (hookThread_.joinable()) {
        hookThread_.join();
    }
    engine_.StopThread();
    if (g_mainWindow == this) {
        g_mainWindow = nullptr;
    }
}

bool MainWindow::Initialize() {
    setupUi();
    loadConfig();

    if (!driver_.LoadFromDirectory(exeDir_.wstring())) {
        std::wstring errorMsg = L"错误原因: " + driver_.GetLastError() + L"\n\n请确认：\n1. 是否已安装 DD 驱动（需要管理员权限运行安装程序）\n2. DD64.dll / DD32.dll 是否在 exe 同目录\n\n程序即将退出。";
        QMessageBox::critical(this, QString::fromStdWString(L"DD 驱动加载失败"),
            QString::fromStdWString(errorMsg));
        return false;
    }

    try {
        setupHooks();
    } catch (const std::exception&) {
        QMessageBox::critical(this, QString::fromStdWString(L"全局钩子初始化失败"),
            QString::fromStdWString(L"无法安装全局键盘/鼠标钩子，程序无法正常监听快捷键。"));
        return false;
    }
    engine_.StartThread();
    applyBindingsToEngine();
    updateHotkeyText();
    updateStartButtonText();
    updateStatusText();

    return true;
}

void MainWindow::setupUi() {
    setWindowTitle(QString::fromStdWString(L"小叽按键~"));
    setWindowIcon(QIcon(":/assets/icon.png"));
    resize(720, 500);
    setMinimumSize(720, 500);

    QWidget* centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    QVBoxLayout* mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setContentsMargins(16, 16, 16, 8);
    mainLayout->setSpacing(16);

    QHBoxLayout* contentLayout = new QHBoxLayout();
    contentLayout->setSpacing(20);

    // Left side: Table
    tableWidget_ = new QTableWidget(0, 4, this);
    tableWidget_->setHorizontalHeaderLabels({
        QString::fromStdWString(L"状态"),
        QString::fromStdWString(L"按键"),
        QString::fromStdWString(L"模式"),
        QString::fromStdWString(L"延迟")
    });
    tableWidget_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    tableWidget_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    tableWidget_->setColumnWidth(0, 50);
    tableWidget_->setSelectionBehavior(QAbstractItemView::SelectRows);
    tableWidget_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tableWidget_->verticalHeader()->setVisible(false);
    tableWidget_->setFocusPolicy(Qt::NoFocus);
    contentLayout->addWidget(tableWidget_, 5);

    // Right side: Controls
    QVBoxLayout* rightLayout = new QVBoxLayout();
    contentLayout->addLayout(rightLayout, 4);

    QHBoxLayout* btnLayout = new QHBoxLayout();
    addBtn_ = new QPushButton(QString::fromStdWString(L"添加按键"), this);
    deleteBtn_ = new QPushButton(QString::fromStdWString(L"删除按键"), this);
    addBtn_->setMinimumHeight(32);
    deleteBtn_->setMinimumHeight(32);
    btnLayout->addWidget(addBtn_);
    btnLayout->addWidget(deleteBtn_);
    rightLayout->addLayout(btnLayout);
    
    rightLayout->addSpacing(24);

    QGridLayout* formLayout = new QGridLayout();
    formLayout->setSpacing(16);

    QLabel* hotkeyLabel = new QLabel(QString::fromStdWString(L"启停快捷键:"), this);
    formLayout->addWidget(hotkeyLabel, 0, 0);
    hotkeyEdit_ = new QLineEdit(this);
    hotkeyEdit_->setReadOnly(true);
    hotkeyEdit_->setPlaceholderText(QString::fromStdWString(L"点击录制"));
    hotkeyEdit_->setMinimumHeight(28);
    formLayout->addWidget(hotkeyEdit_, 0, 1);

    rightLayout->addLayout(formLayout);
    rightLayout->addSpacing(24);

    beepCheck_ = new QCheckBox(QString::fromStdWString(L"开启提示音"), this);
    rightLayout->addWidget(beepCheck_);

    QPushButton* settingsBtn = new QPushButton(QString::fromStdWString(L"设置"), this);
    settingsBtn->setMinimumHeight(28);
    rightLayout->addWidget(settingsBtn);

    rightLayout->addStretch();

    startBtn_ = new QPushButton(QString::fromStdWString(L"启动"), this);
    startBtn_->setMinimumHeight(48);
    QFont startFont = startBtn_->font();
    startFont.setPointSize(11);
    startFont.setBold(true);
    startBtn_->setFont(startFont);
    rightLayout->addWidget(startBtn_);

    mainLayout->addLayout(contentLayout);

    QHBoxLayout* bottomLayout = new QHBoxLayout();
    statusLabel_ = new QLabel(this);
    bottomLayout->addWidget(statusLabel_);
    bottomLayout->addStretch();
    QLabel* authorLabel = new QLabel(QString::fromStdWString(L"By：龙虎-闻成画"), this);
    bottomLayout->addWidget(authorLabel);
    mainLayout->addLayout(bottomLayout);

    connect(tableWidget_, &QTableWidget::cellDoubleClicked, this, &MainWindow::onListDoubleClicked);
    connect(tableWidget_, &QTableWidget::itemChanged, this, &MainWindow::onItemChanged);
    connect(addBtn_, &QPushButton::clicked, this, &MainWindow::onAddBinding);
    connect(deleteBtn_, &QPushButton::clicked, this, &MainWindow::onDeleteSelected);
    connect(startBtn_, &QPushButton::clicked, this, &MainWindow::onStartClicked);
    connect(beepCheck_, &QCheckBox::toggled, this, &MainWindow::onBeepToggled);
    connect(settingsBtn, &QPushButton::clicked, this, &MainWindow::onSettingsClicked);

    hotkeyEdit_->installEventFilter(this);
    
    // Install event filter to capture focus events on inputs
    qApp->installEventFilter(this);

    connect(this, &MainWindow::capturedKey, this, &MainWindow::handleCapturedKey);
    connect(this, &MainWindow::holdStateChanged, this, &MainWindow::updateStatusText);
    connect(this, &MainWindow::toolStateChanged, this, &MainWindow::handleToolStateChanged);

    // 系统托盘
    trayIcon_ = new QSystemTrayIcon(QIcon(":/assets/icon.png"), this);
    trayMenu_ = new QMenu(this);
    QAction* showAction = trayMenu_->addAction(QString::fromStdWString(L"显示主窗口"));
    QAction* quitAction = trayMenu_->addAction(QString::fromStdWString(L"退出"));
    trayIcon_->setContextMenu(trayMenu_);
    trayIcon_->setToolTip(QString::fromStdWString(L"小叽按键"));
    trayIcon_->show();

    connect(showAction, &QAction::triggered, this, [this]() {
        showNormal();
        activateWindow();
    });
    connect(quitAction, &QAction::triggered, this, [this]() {
        trayIcon_->hide();
        qApp->quit();
    });
    connect(trayIcon_, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::DoubleClick) {
            showNormal();
            activateWindow();
        }
    });
}

void MainWindow::setupHooks() {
    {
        std::lock_guard lock(hookInitMutex_);
        hookInitCompleted_ = false;
        hookInitSucceeded_ = false;
    }

    hookThread_ = std::thread(&MainWindow::runHookLoop, this);

    std::unique_lock lock(hookInitMutex_);
    hookInitCv_.wait(lock, [&] { return hookInitCompleted_; });
    if (!hookInitSucceeded_) {
        throw std::runtime_error("failed to install global hooks");
    }
}

void MainWindow::runHookLoop() {
    MSG msg;
    PeekMessageW(&msg, nullptr, WM_USER, WM_USER, PM_NOREMOVE);

    hookThreadId_ = GetCurrentThreadId();
    keyboardHook_ = SetWindowsHookExW(WH_KEYBOARD_LL, &MainWindow::KeyboardHookProc, nullptr, 0);
    mouseHook_ = SetWindowsHookExW(WH_MOUSE_LL, &MainWindow::MouseHookProc, nullptr, 0);

    {
        std::lock_guard lock(hookInitMutex_);
        hookInitSucceeded_ = (keyboardHook_ != nullptr && mouseHook_ != nullptr);
        hookInitCompleted_ = true;
    }
    hookInitCv_.notify_one();

    if (!hookInitSucceeded_) {
        if (keyboardHook_ != nullptr) {
            UnhookWindowsHookEx(keyboardHook_);
            keyboardHook_ = nullptr;
        }
        if (mouseHook_ != nullptr) {
            UnhookWindowsHookEx(mouseHook_);
            mouseHook_ = nullptr;
        }
        hookThreadId_ = 0;
        return;
    }

    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    if (keyboardHook_ != nullptr) {
        UnhookWindowsHookEx(keyboardHook_);
        keyboardHook_ = nullptr;
    }
    if (mouseHook_ != nullptr) {
        UnhookWindowsHookEx(mouseHook_);
        mouseHook_ = nullptr;
    }
    hookThreadId_ = 0;
}

void MainWindow::loadConfig() {
    config_ = configStore_.Load();
    nextBindingId_ = 1;
    for (auto& binding : config_.bindings) {
        if (binding.id == 0) {
            binding.id = nextBindingId_++;
            continue;
        }
        nextBindingId_ = std::max(nextBindingId_, binding.id + 1);
    }

    beepCheck_->setChecked(config_.beepEnabled);
    updateHotkeyText();
    refreshList();
    refreshHookState();
}

void MainWindow::saveConfig() {
    std::wstring error;
    if (!configStore_.Save(config_, error)) {
        QMessageBox::warning(this, QString::fromStdWString(L"保存配置失败"), QString::fromStdWString(error));
    }
}

void MainWindow::applyBindingsToEngine() {
    engine_.UpdateBindings(config_.bindings);
    refreshHookState();
    updateStatusText();
}

void MainWindow::refreshList() {
    tableWidget_->blockSignals(true);
    tableWidget_->setRowCount(0);

    for (int i = 0; i < static_cast<int>(config_.bindings.size()); ++i) {
        const auto& binding = config_.bindings[static_cast<std::size_t>(i)];
        tableWidget_->insertRow(i);

        QTableWidgetItem* stateItem = new QTableWidgetItem();
        stateItem->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        stateItem->setCheckState(binding.enabled ? Qt::Checked : Qt::Unchecked);
        stateItem->setData(Qt::UserRole, static_cast<qulonglong>(binding.id));
        tableWidget_->setItem(i, 0, stateItem);

        QTableWidgetItem* keyItem = new QTableWidgetItem(QString::fromStdWString(GetKeyDisplayName(binding.key)));
        tableWidget_->setItem(i, 1, keyItem);

        QTableWidgetItem* modeItem = new QTableWidgetItem(QString::fromStdWString(RepeatModeToText(binding.mode)));
        tableWidget_->setItem(i, 2, modeItem);

        QTableWidgetItem* delayItem = new QTableWidgetItem(QString::number(binding.delayMs));
        tableWidget_->setItem(i, 3, delayItem);
    }
    tableWidget_->blockSignals(false);
}

void MainWindow::updateHotkeyText() {
    QString text = captureRequest_.has_value() && captureRequest_->target == CaptureRequest::Target::MainHotkey
        ? QString::fromStdWString(L"请按下一个键...")
        : (config_.controlHotkey.has_value() ? QString::fromStdWString(GetKeyDisplayName(*config_.controlHotkey)) : QString::fromStdWString(L"None"));
    hotkeyEdit_->setText(text);
}

void MainWindow::updateStatusText() {
    QString status = engine_.IsToolRunning() ? QString::fromStdWString(L"已启动") : QString::fromStdWString(L"已停止");
    QString hotkey = config_.controlHotkey.has_value() ? QString::fromStdWString(GetKeyDisplayName(*config_.controlHotkey)) : QString::fromStdWString(L"无");

#ifndef APP_VERSION
#define APP_VERSION "2.0.0"
#endif

    QString text = QString::fromStdWString(L"版本: v") + QString(APP_VERSION) + QString::fromStdWString(L" | 状态: ") + status + QString::fromStdWString(L" | 当前控制键: ") + hotkey;
    statusLabel_->setText(text);
}

void MainWindow::updateStartButtonText() {
    startBtn_->setText(engine_.IsToolRunning() ? QString::fromStdWString(L"停止") : QString::fromStdWString(L"启动"));
}

void MainWindow::toggleToolRunning() {
    setToolRunning(!engine_.IsToolRunning());
}

void MainWindow::setToolRunning(bool running) {
    if (!setToolRunningFast(running)) {
        return;
    }
    handleToolStateChanged(running);
}

bool MainWindow::setToolRunningFast(bool running) {
    if (engine_.IsToolRunning() == running) {
        return false;
    }

    controlHotkeyPressed_ = false;
    engine_.SetToolRunning(running);
    return true;
}

void MainWindow::handleToolStateChanged(bool running) {
    updateStartButtonText();
    updateStatusText();
    playToggleSound(running);
}

void MainWindow::playToggleSound(bool running) const {
    if (!config_.beepEnabled) {
        return;
    }

    const auto soundPath = resolveAudioPath(running ? L"按键开启.mp3" : L"按键关闭.mp3");
    if (soundPath.empty()) {
        return;
    }

    mciSendStringW(L"close autokey_toggle_sound", nullptr, 0, nullptr);
    const std::wstring openCommand = L"open \"" + soundPath.wstring() + L"\" type mpegvideo alias autokey_toggle_sound";
    if (mciSendStringW(openCommand.c_str(), nullptr, 0, nullptr) != 0) {
        return;
    }
    mciSendStringW(L"play autokey_toggle_sound from 0", nullptr, 0, nullptr);
}

std::filesystem::path MainWindow::resolveAudioPath(const wchar_t* fileName) const {
    const auto parent1 = exeDir_.parent_path();
    const auto parent2 = parent1.parent_path();
    const std::array candidates{
        exeDir_ / L"audio" / fileName,
        exeDir_ / L"assets" / L"audio" / fileName,
        parent1 / L"assets" / L"audio" / fileName,
        parent2 / L"assets" / L"audio" / fileName,
    };

    for (const auto& candidate : candidates) {
        if (!candidate.empty() && std::filesystem::exists(candidate)) {
            return candidate;
        }
    }
    return {};
}

bool MainWindow::IsHoldBindingEnabledForKey(const KeyDescriptor& key) const {
    return std::any_of(config_.bindings.begin(), config_.bindings.end(), [&](const KeyBinding& binding) {
        return binding.enabled && binding.mode == RepeatMode::Hold && binding.key == key;
    });
}

bool MainWindow::IsHoldBindingEnabledForKeyRealtime(const KeyDescriptor& key) const {
    const auto& bindings = config_.bindings;
    return std::any_of(bindings.begin(), bindings.end(), [&](const KeyBinding& b) {
        return b.enabled && b.mode == RepeatMode::Hold && b.key == key;
    });
}

void MainWindow::onAddBinding() {
    openEditDialog(std::nullopt);
}

void MainWindow::onDeleteSelected() {
    std::vector<std::uint64_t> toDelete;
    for (int i = 0; i < tableWidget_->rowCount(); ++i) {
        if (tableWidget_->item(i, 0)->isSelected() || tableWidget_->item(i, 1)->isSelected()) {
            toDelete.push_back(static_cast<std::uint64_t>(tableWidget_->item(i, 0)->data(Qt::UserRole).toULongLong()));
        }
    }
    if (toDelete.empty()) {
        return;
    }

    config_.bindings.erase(
        std::remove_if(config_.bindings.begin(), config_.bindings.end(), [&](const KeyBinding& binding) {
            return std::find(toDelete.begin(), toDelete.end(), binding.id) != toDelete.end();
        }),
        config_.bindings.end());

    refreshList();
    applyBindingsToEngine();
    saveConfig();
}

void MainWindow::onStartClicked() {
    toggleToolRunning();
}

void MainWindow::onHotkeyEditClicked() {
    captureRequest_ = CaptureRequest{CaptureRequest::Target::MainHotkey};
    refreshHookState();
    {
        std::lock_guard lock(hookStateMutex_);
        hookCaptureStartedTick_ = GetTickCount64();
    }
    updateHotkeyText();
}

void MainWindow::onBeepToggled() {
    config_.beepEnabled = beepCheck_->isChecked();
    saveConfig();
}

void MainWindow::onSettingsClicked() {
    QDialog dialog(this);
    dialog.setWindowTitle(QString::fromStdWString(L"设置"));
    dialog.setWindowIcon(QIcon(":/assets/icon.png"));
    dialog.setFixedSize(460, 440);

    QVBoxLayout* mainLayout = new QVBoxLayout(&dialog);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(12);

    QTabWidget* tabWidget = new QTabWidget(&dialog);
    mainLayout->addWidget(tabWidget);

    // ==================== 设置页 ====================
    QWidget* settingsPage = new QWidget();
    QVBoxLayout* settingsLayout = new QVBoxLayout(settingsPage);
    settingsLayout->setContentsMargins(16, 16, 16, 8);
    settingsLayout->setSpacing(16);

    // 关闭行为
    QGroupBox* closeGroup = new QGroupBox(QString::fromStdWString(L"关闭窗口时"), settingsPage);
    QVBoxLayout* closeLayout = new QVBoxLayout(closeGroup);

    QRadioButton* radioAsk = new QRadioButton(QString::fromStdWString(L"每次询问"), closeGroup);
    QRadioButton* radioMinimize = new QRadioButton(QString::fromStdWString(L"最小化到系统托盘"), closeGroup);
    QRadioButton* radioExit = new QRadioButton(QString::fromStdWString(L"直接退出程序"), closeGroup);

    if (config_.closeAction == CloseAction::Minimize) radioMinimize->setChecked(true);
    else if (config_.closeAction == CloseAction::Exit) radioExit->setChecked(true);
    else radioAsk->setChecked(true);

    closeLayout->addWidget(radioAsk);
    closeLayout->addWidget(radioMinimize);
    closeLayout->addWidget(radioExit);
    settingsLayout->addWidget(closeGroup);

    // 配置文件
    QGroupBox* configGroup = new QGroupBox(QString::fromStdWString(L"配置文件"), settingsPage);
    QVBoxLayout* configLayout = new QVBoxLayout(configGroup);

    QLabel* configPathLabel = new QLabel(QString::fromStdWString(configStore_.GetPath().wstring()), configGroup);
    configPathLabel->setWordWrap(true);
    configPathLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    configLayout->addWidget(configPathLabel);

    QHBoxLayout* configBtnLayout = new QHBoxLayout();
    QPushButton* openConfigFolderBtn = new QPushButton(QString::fromStdWString(L"打开文件夹"), configGroup);
    QPushButton* changeConfigPathBtn = new QPushButton(QString::fromStdWString(L"自定义路径"), configGroup);
    QPushButton* resetConfigPathBtn = new QPushButton(QString::fromStdWString(L"恢复默认"), configGroup);
    openConfigFolderBtn->setMinimumHeight(28);
    changeConfigPathBtn->setMinimumHeight(28);
    resetConfigPathBtn->setMinimumHeight(28);
    configBtnLayout->addWidget(openConfigFolderBtn);
    configBtnLayout->addWidget(changeConfigPathBtn);
    configBtnLayout->addWidget(resetConfigPathBtn);
    configLayout->addLayout(configBtnLayout);
    settingsLayout->addWidget(configGroup);

    settingsLayout->addStretch();

    connect(openConfigFolderBtn, &QPushButton::clicked, [this]() {
        const auto folder = configStore_.GetPath().parent_path();
        QDesktopServices::openUrl(QUrl::fromLocalFile(QString::fromStdWString(folder.wstring())));
    });

    connect(changeConfigPathBtn, &QPushButton::clicked, [this, configPathLabel]() {
        QString filePath = QFileDialog::getSaveFileName(this,
            QString::fromStdWString(L"选择配置文件路径"),
            QString::fromStdWString(configStore_.GetPath().wstring()),
            QString::fromStdWString(L"JSON 文件 (*.json)"));
        if (filePath.isEmpty()) return;

        std::filesystem::path newPath = filePath.toStdWString();
        // 确保目录存在
        std::filesystem::create_directories(newPath.parent_path());

        // 将当前配置保存到新路径
        config_.customConfigPath = newPath.wstring();
        configStore_.SetPath(newPath);
        std::wstring error;
        if (!configStore_.Save(config_, error)) {
            QMessageBox::warning(this, QString::fromStdWString(L"保存失败"), QString::fromStdWString(error));
            return;
        }

        // 同时更新默认位置的指向记录
        saveDefaultConfigPointer();
        configPathLabel->setText(QString::fromStdWString(newPath.wstring()));
    });

    connect(resetConfigPathBtn, &QPushButton::clicked, [this, configPathLabel]() {
        std::filesystem::path defaultPath = getDefaultConfigPath();
        if (configStore_.GetPath() == defaultPath) return;

        config_.customConfigPath.clear();
        configStore_.SetPath(defaultPath);
        std::wstring error;
        if (!configStore_.Save(config_, error)) {
            QMessageBox::warning(this, QString::fromStdWString(L"保存失败"), QString::fromStdWString(error));
            return;
        }
        saveDefaultConfigPointer();
        configPathLabel->setText(QString::fromStdWString(defaultPath.wstring()));
    });

    tabWidget->addTab(settingsPage, QString::fromStdWString(L"设置"));

    // ==================== 关于页 ====================
    QWidget* aboutPage = new QWidget();
    QVBoxLayout* aboutLayout = new QVBoxLayout(aboutPage);
    aboutLayout->setContentsMargins(16, 16, 16, 8);
    aboutLayout->setSpacing(16);

    // 联系作者
    QGroupBox* contactGroup = new QGroupBox(QString::fromStdWString(L"联系作者"), aboutPage);
    QGridLayout* contactLayout = new QGridLayout(contactGroup);
    contactLayout->setSpacing(8);

    contactLayout->addWidget(new QLabel("QQ:", contactGroup), 0, 0);
    QLineEdit* qqEdit = new QLineEdit("2033653725", contactGroup);
    qqEdit->setReadOnly(true);
    contactLayout->addWidget(qqEdit, 0, 1);
    QPushButton* copyQqBtn = new QPushButton(QString::fromStdWString(L"复制"), contactGroup);
    contactLayout->addWidget(copyQqBtn, 0, 2);

    contactLayout->addWidget(new QLabel(QString::fromStdWString(L"邮箱:"), contactGroup), 1, 0);
    QLineEdit* emailEdit = new QLineEdit("2033653725@qq.com", contactGroup);
    emailEdit->setReadOnly(true);
    contactLayout->addWidget(emailEdit, 1, 1);
    QPushButton* copyEmailBtn = new QPushButton(QString::fromStdWString(L"复制"), contactGroup);
    contactLayout->addWidget(copyEmailBtn, 1, 2);

    aboutLayout->addWidget(contactGroup);

    // 免责声明
    QGroupBox* disclaimerGroup = new QGroupBox(QString::fromStdWString(L"免责声明"), aboutPage);
    QVBoxLayout* disclaimerLayout = new QVBoxLayout(disclaimerGroup);
    QLabel* disclaimerLabel = new QLabel(
        QString::fromStdWString(
            L"1. 本软件仅供学习和交流使用，请勿用于商业用途\n"
            L"2. 使用本软件产生的一切后果由用户自行承担\n"
            L"3. 作者不对软件使用造成的任何损失负责\n"
            L"4. 禁止使用本软件进行任何违法活动"),
        disclaimerGroup);
    disclaimerLabel->setWordWrap(true);
    disclaimerLayout->addWidget(disclaimerLabel);
    aboutLayout->addWidget(disclaimerGroup);

    aboutLayout->addStretch();

    connect(copyQqBtn, &QPushButton::clicked, [qqEdit]() {
        QApplication::clipboard()->setText(qqEdit->text());
    });
    connect(copyEmailBtn, &QPushButton::clicked, [emailEdit]() {
        QApplication::clipboard()->setText(emailEdit->text());
    });

    tabWidget->addTab(aboutPage, QString::fromStdWString(L"关于"));

    // ==================== 底部按钮 ====================
    QHBoxLayout* btnLayout = new QHBoxLayout();
    btnLayout->addStretch();
    QPushButton* okBtn = new QPushButton(QString::fromStdWString(L"确定"), &dialog);
    okBtn->setMinimumSize(80, 32);
    btnLayout->addWidget(okBtn);
    mainLayout->addLayout(btnLayout);

    connect(okBtn, &QPushButton::clicked, &dialog, &QDialog::accept);

    dialog.exec();

    // 保存关闭行为设置
    CloseAction newAction = CloseAction::Ask;
    if (radioMinimize->isChecked()) newAction = CloseAction::Minimize;
    else if (radioExit->isChecked()) newAction = CloseAction::Exit;

    if (newAction != config_.closeAction) {
        config_.closeAction = newAction;
        saveConfig();
    }
}

void MainWindow::onListDoubleClicked(int row, int /*col*/) {
    auto id = static_cast<std::uint64_t>(tableWidget_->item(row, 0)->data(Qt::UserRole).toULongLong());
    openEditDialog(id);
}

void MainWindow::onItemChanged(QTableWidgetItem* item) {
    if (item->column() == 0) {
        auto id = static_cast<std::uint64_t>(item->data(Qt::UserRole).toULongLong());
        if (auto idx = findBindingIndex(id)) {
            config_.bindings[*idx].enabled = (item->checkState() == Qt::Checked);
            applyBindingsToEngine();
            saveConfig();
        }
    }
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event) {
    if (event->type() == QEvent::MouseButtonPress) {
        if (watched == hotkeyEdit_) {
            onHotkeyEditClicked();
            return true;
        } else if (watched == editKeyBox_) {
            currentEditKey_ = KeyDescriptor{};
            editKeyBox_->setText(QString::fromStdWString(L"点击录制"));
            captureRequest_ = CaptureRequest{CaptureRequest::Target::EditDialogKey};
            refreshHookState();
            {
                std::lock_guard lock(hookStateMutex_);
                hookCaptureStartedTick_ = GetTickCount64();
            }
            return true;
        } else if (captureRequest_.has_value()) {
            // Cancel capture if clicking outside
            captureRequest_.reset();
            refreshHookState();
            updateHotkeyText();
            if (editKeyBox_) {
                editKeyBox_->setText(currentEditKey_.IsValid() ? QString::fromStdWString(GetKeyDisplayName(currentEditKey_)) : QString::fromStdWString(L"点击录制"));
            }
        }
    }
    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::openEditDialog(std::optional<std::uint64_t> bindingId) {
    if (editDialog_) {
        editDialog_->activateWindow();
        return;
    }

    QDialog dialog(this);
    dialog.setWindowTitle(bindingId.has_value() ? QString::fromStdWString(L"编辑按键") : QString::fromStdWString(L"添加按键"));
    dialog.setFixedSize(360, 240);

    QVBoxLayout* layout = new QVBoxLayout(&dialog);
    QGridLayout* form = new QGridLayout();

    form->addWidget(new QLabel(QString::fromStdWString(L"按键:")), 0, 0);
    editKeyBox_ = new QLineEdit();
    editKeyBox_->setReadOnly(true);
    form->addWidget(editKeyBox_, 0, 1);

    form->addWidget(new QLabel(QString::fromStdWString(L"模式:")), 1, 0);
    editModeCombo_ = new QComboBox();
    editModeCombo_->addItem(QString::fromStdWString(L"开关模式"));
    editModeCombo_->addItem(QString::fromStdWString(L"按压模式"));
    form->addWidget(editModeCombo_, 1, 1);

    form->addWidget(new QLabel(QString::fromStdWString(L"延迟(ms):")), 2, 0);
    editDelayEdit_ = new QLineEdit();
    form->addWidget(editDelayEdit_, 2, 1);

    editEnabledCheck_ = new QCheckBox(QString::fromStdWString(L"启用此按键"));
    form->addWidget(editEnabledCheck_, 3, 1);

    layout->addLayout(form);

    QDialogButtonBox* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    layout->addWidget(buttons);

    editDialog_ = &dialog;
    editingExisting_ = bindingId.has_value();
    editBindingId_ = bindingId.value_or(0);
    currentEditKey_ = KeyDescriptor{};

    if (editingExisting_) {
        if (auto idx = findBindingIndex(editBindingId_)) {
            const auto& b = config_.bindings[*idx];
            currentEditKey_ = b.key;
            editModeCombo_->setCurrentIndex(b.mode == RepeatMode::Hold ? 1 : 0);
            editDelayEdit_->setText(QString::number(b.delayMs));
            editEnabledCheck_->setChecked(b.enabled);
        }
    } else {
        editModeCombo_->setCurrentIndex(0);
        editDelayEdit_->setText(QString::number(config_.defaultDelayMs));
        editEnabledCheck_->setChecked(true);
    }

    editKeyBox_->setText(currentEditKey_.IsValid() ? QString::fromStdWString(GetKeyDisplayName(currentEditKey_)) : QString::fromStdWString(L"点击录制"));

    editKeyBox_->installEventFilter(this);

    connect(buttons, &QDialogButtonBox::accepted, [&]() {
        if (!currentEditKey_.IsValid()) {
            QMessageBox::warning(&dialog, QString::fromStdWString(L"参数无效"), QString::fromStdWString(L"请先录制一个按键或鼠标键。"));
            return;
        }
        bool ok;
        int delay = editDelayEdit_->text().toInt(&ok);
        if (!ok || delay <= 0) {
            QMessageBox::warning(&dialog, QString::fromStdWString(L"参数无效"), QString::fromStdWString(L"延迟必须是大于 0 的整数。"));
            return;
        }

        // ⭐ 检查是否与启停快捷键冲突
        if (config_.controlHotkey.has_value() && *config_.controlHotkey == currentEditKey_) {
            QMessageBox::warning(&dialog, QString::fromStdWString(L"按键冲突"),
                QString::fromStdWString(L"该按键已被设置为启停快捷键，不能同时作为连发/按压按键。"));
            return;
        }

        // ⭐ 检查是否与已有绑定重复（编辑时排除自身）
        for (const auto& b : config_.bindings) {
            if (b.key == currentEditKey_) {
                if (editingExisting_ && b.id == editBindingId_) continue;
                QMessageBox::warning(&dialog, QString::fromStdWString(L"按键重复"),
                    QString::fromStdWString(L"该按键已被配置，不允许重复添加。"));
                return;
            }
        }

        if (editingExisting_) {
            if (auto idx = findBindingIndex(editBindingId_)) {
                auto& b = config_.bindings[*idx];
                b.key = currentEditKey_;
                b.mode = editModeCombo_->currentIndex() == 1 ? RepeatMode::Hold : RepeatMode::Toggle;
                b.delayMs = delay;
                b.enabled = editEnabledCheck_->isChecked();
            }
        } else {
            KeyBinding b;
            b.id = nextBindingId_++;
            b.key = currentEditKey_;
            b.mode = editModeCombo_->currentIndex() == 1 ? RepeatMode::Hold : RepeatMode::Toggle;
            b.delayMs = delay;
            b.enabled = editEnabledCheck_->isChecked();
            config_.bindings.push_back(b);
        }

        refreshList();
        applyBindingsToEngine();
        saveConfig();
        dialog.accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    dialog.exec();
    
    captureRequest_.reset();
    refreshHookState();
    updateHotkeyText();
    editDialog_ = nullptr;
    editKeyBox_ = nullptr;
}

void MainWindow::handleCapturedKey(quint64 encodedKey) {
    onCapturedKeyInternal(DecodeKey(encodedKey));
}

void MainWindow::onCapturedKeyInternal(const KeyDescriptor& key) {
    if (!captureRequest_.has_value()) {
        return;
    }

    if (captureRequest_->target == CaptureRequest::Target::MainHotkey) {
        // ⭐ 先取消录制状态，再做后续处理（防止弹窗时鼠标点击触发新的录制）
        captureRequest_.reset();
        refreshHookState();

        if (key.device == InputDevice::Mouse && (key.code == VK_LBUTTON || key.code == VK_RBUTTON)) {
            QMessageBox::warning(this, QString::fromStdWString(L"按键无效"),
                QString::fromStdWString(L"为了防止影响正常操作，启停快捷键不能设置为鼠标左键或右键。\n请使用键盘按键或鼠标侧键。"));
        } else {
            // ⭐ 检查是否与已有绑定冲突
            bool conflict = std::any_of(config_.bindings.begin(), config_.bindings.end(), [&](const KeyBinding& b) {
                return b.key == key;
            });
            if (conflict) {
                QMessageBox::warning(this, QString::fromStdWString(L"按键冲突"),
                    QString::fromStdWString(L"该按键已被配置为连发/按压按键，不能同时作为启停快捷键。"));
            } else {
                config_.controlHotkey = key;
                saveConfig();
                refreshHookState();  // ⭐ 立即同步到钩子线程，使新快捷键即时生效
            }
        }
        updateHotkeyText();
        updateStatusText();  // ⭐ 同步更新底部状态栏
    } else {
        currentEditKey_ = key;
        if (editKeyBox_) {
            editKeyBox_->setText(QString::fromStdWString(GetKeyDisplayName(key)));
        }
        captureRequest_.reset();
        refreshHookState();
    }
}

void MainWindow::closeEvent(QCloseEvent *event) {
    saveConfig();

    if (config_.closeAction == CloseAction::Minimize) {
        hide();
        event->ignore();
        return;
    }
    if (config_.closeAction == CloseAction::Exit) {
        trayIcon_->hide();
        event->accept();
        return;
    }

    // CloseAction::Ask — 弹出询问对话框
    QDialog dialog(this);
    dialog.setWindowTitle(QString::fromStdWString(L"关闭确认"));
    dialog.setFixedSize(320, 160);

    QVBoxLayout* layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(20, 16, 20, 12);
    layout->setSpacing(12);

    QLabel* label = new QLabel(QString::fromStdWString(L"您希望关闭程序还是最小化到系统托盘？"), &dialog);
    label->setWordWrap(true);
    layout->addWidget(label);

    QCheckBox* rememberCheck = new QCheckBox(QString::fromStdWString(L"记住我的选择"), &dialog);
    layout->addWidget(rememberCheck);

    QHBoxLayout* btnLayout = new QHBoxLayout();
    btnLayout->addStretch();
    QPushButton* minimizeBtn = new QPushButton(QString::fromStdWString(L"最小化到托盘"), &dialog);
    QPushButton* exitBtn = new QPushButton(QString::fromStdWString(L"退出程序"), &dialog);
    minimizeBtn->setMinimumHeight(28);
    exitBtn->setMinimumHeight(28);
    btnLayout->addWidget(minimizeBtn);
    btnLayout->addWidget(exitBtn);
    layout->addLayout(btnLayout);

    bool chosenExit = false;
    connect(minimizeBtn, &QPushButton::clicked, [&]() { chosenExit = false; dialog.accept(); });
    connect(exitBtn, &QPushButton::clicked, [&]() { chosenExit = true; dialog.accept(); });

    dialog.exec();

    if (rememberCheck->isChecked()) {
        config_.closeAction = chosenExit ? CloseAction::Exit : CloseAction::Minimize;
        saveConfig();
    }

    if (chosenExit) {
        trayIcon_->hide();
        event->accept();
    } else {
        hide();
        event->ignore();
    }
}

void MainWindow::refreshHookState() {
    std::lock_guard lock(hookStateMutex_);
    hookControlHotkey_ = config_.controlHotkey;
    hookCaptureActive_ = captureRequest_.has_value();
    if (!hookCaptureActive_) {
        hookCaptureStartedTick_ = 0;
    }
}

std::optional<std::size_t> MainWindow::findBindingIndex(std::uint64_t id) const {
    for (std::size_t i = 0; i < config_.bindings.size(); ++i) {
        if (config_.bindings[i].id == id) {
            return i;
        }
    }
    return std::nullopt;
}

std::wstring MainWindow::getExeDir() const {
    std::wstring path(MAX_PATH, L'\0');
    const DWORD length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
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
    // 默认路径下检查是否有自定义路径指向
    std::filesystem::path defaultPath = getDefaultConfigPath();
    std::filesystem::path pointerFile = defaultPath.parent_path() / L"config_path.txt";

    if (std::filesystem::exists(pointerFile)) {
        std::wifstream ifs(pointerFile);
        std::wstring customPath;
        if (std::getline(ifs, customPath) && !customPath.empty() && std::filesystem::exists(customPath)) {
            return customPath;
        }
    }
    return defaultPath;
}

void MainWindow::saveDefaultConfigPointer() const {
    std::filesystem::path defaultPath = getDefaultConfigPath();
    std::filesystem::path pointerFile = defaultPath.parent_path() / L"config_path.txt";

    if (config_.customConfigPath.empty()) {
        // 删除指针文件
        std::filesystem::remove(pointerFile);
    } else {
        std::wofstream ofs(pointerFile, std::ios::trunc);
        ofs << config_.customConfigPath;
    }
}

LRESULT CALLBACK MainWindow::KeyboardHookProc(int code, WPARAM wParam, LPARAM lParam) {
    if (code < 0 || g_mainWindow == nullptr) {
        return CallNextHookEx(nullptr, code, wParam, lParam);
    }

    const auto* info = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);
    KeyDescriptor key{InputDevice::Keyboard, info->vkCode};
    const bool pressed = wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN;
    const bool released = wParam == WM_KEYUP || wParam == WM_SYSKEYUP;
    if (!pressed && !released) {
        return CallNextHookEx(nullptr, code, wParam, lParam);
    }

    if (info->flags & LLKHF_INJECTED)
        return CallNextHookEx(nullptr, code, wParam, lParam);

    // 实时查询 Engine 状态，不依赖缓存
    const bool toolRunning = g_mainWindow->engine_.IsToolRunning();

    std::optional<KeyDescriptor> controlHotkey;
    bool captureActive = false;
    {
        std::lock_guard lock(g_mainWindow->hookStateMutex_);
        controlHotkey = g_mainWindow->hookControlHotkey_;
        captureActive = g_mainWindow->hookCaptureActive_;
    }

    if (captureActive && pressed) {
        const auto encodedKey = EncodeKey(key);
        QMetaObject::invokeMethod(g_mainWindow, [window = g_mainWindow, encodedKey] {
            window->handleCapturedKey(encodedKey);
        }, Qt::QueuedConnection);
        return 1;
    }

    const bool isControlHotkey = controlHotkey.has_value() && *controlHotkey == key;

    if (isControlHotkey) {
        if (pressed && !g_mainWindow->controlHotkeyPressed_.exchange(true)) {
            const bool newRunning = !toolRunning;
            if (g_mainWindow->setToolRunningFast(newRunning)) {
                QMetaObject::invokeMethod(g_mainWindow, [window = g_mainWindow, newRunning] {
                    window->handleToolStateChanged(newRunning);
                }, Qt::QueuedConnection);
            }
        } else if (released) {
            g_mainWindow->controlHotkeyPressed_ = false;
        }
    }

    // ⭐ 通过Hook追踪物理按键状态（OnPhysicalDown/Up内部已通过per-key时间戳过滤SendTap事件）
    if (toolRunning) {
        if (pressed) {
            g_mainWindow->engine_.OnPhysicalDown(key);
        } else if (released) {
            g_mainWindow->engine_.OnPhysicalUp(key);
        }
    }

    return CallNextHookEx(nullptr, code, wParam, lParam);
}

LRESULT CALLBACK MainWindow::MouseHookProc(int code, WPARAM wParam, LPARAM lParam) {
    if (code < 0 || g_mainWindow == nullptr) {
        return CallNextHookEx(nullptr, code, wParam, lParam);
    }

    const auto* info = reinterpret_cast<MSLLHOOKSTRUCT*>(lParam);
    if (!IsMouseDownMessage(wParam) && !IsMouseUpMessage(wParam)) {
        return CallNextHookEx(nullptr, code, wParam, lParam);
    }

    const KeyDescriptor key = MouseMessageToDescriptor(wParam, *info);
    if (!key.IsValid()) {
        return CallNextHookEx(nullptr, code, wParam, lParam);
    }

    if (info->flags & LLMHF_INJECTED)
        return CallNextHookEx(nullptr, code, wParam, lParam);

    const bool pressed = IsMouseDownMessage(wParam);
    const bool released = IsMouseUpMessage(wParam);

    // 实时查询 Engine 状态，不依赖缓存
    const bool toolRunning = g_mainWindow->engine_.IsToolRunning();

    std::optional<KeyDescriptor> controlHotkey;
    bool captureActive = false;
    ULONGLONG captureStartedTick = 0;
    {
        std::lock_guard lock(g_mainWindow->hookStateMutex_);
        controlHotkey = g_mainWindow->hookControlHotkey_;
        captureActive = g_mainWindow->hookCaptureActive_;
        captureStartedTick = g_mainWindow->hookCaptureStartedTick_;
    }

    if (captureActive && pressed) {
        if (GetTickCount64() - captureStartedTick < 150) {
            return CallNextHookEx(nullptr, code, wParam, lParam);
        }
        const auto encodedKey = EncodeKey(key);
        QMetaObject::invokeMethod(g_mainWindow, [window = g_mainWindow, encodedKey] {
            window->handleCapturedKey(encodedKey);
        }, Qt::QueuedConnection);
        return 1;
    }

    const bool isControlHotkey = controlHotkey.has_value() && *controlHotkey == key;

    if (isControlHotkey) {
        if (pressed && !g_mainWindow->controlHotkeyPressed_.exchange(true)) {
            const bool newRunning = !toolRunning;
            if (g_mainWindow->setToolRunningFast(newRunning)) {
                QMetaObject::invokeMethod(g_mainWindow, [window = g_mainWindow, newRunning] {
                    window->handleToolStateChanged(newRunning);
                }, Qt::QueuedConnection);
            }
        } else if (released) {
            g_mainWindow->controlHotkeyPressed_ = false;
        }
    }

    // ⭐ 通过Hook追踪物理按键状态（OnPhysicalDown/Up内部已通过per-key时间戳过滤SendTap事件）
    if (toolRunning) {
        if (pressed) {
            g_mainWindow->engine_.OnPhysicalDown(key);
        } else if (released) {
            g_mainWindow->engine_.OnPhysicalUp(key);
        }
    }

    return CallNextHookEx(nullptr, code, wParam, lParam);
}
