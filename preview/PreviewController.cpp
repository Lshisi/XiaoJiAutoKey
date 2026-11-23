#include "PreviewController.h"

#include "FactionTheme.h"
#include "ui/styles/QmlTheme.h"
#include <QDate>
#include <QGuiApplication>
#include <QTimer>
#include <QWindow>
#include <memory>

PreviewController::PreviewController(QObject* parent)
    : QObject(parent), keys_(false, dialog_, overviewDialog_), macros_(true, dialog_, overviewDialog_) {
    FactionTheme::instance().setCurrent(QStringLiteral("qixiu"));
    setDarkMode(true);
    overlayOptions_ = {{"tab", 0}, {"nativeBlur", true}, {"opacity", 70},
        {"repeatHotkey", true}, {"repeatIndependent", true}, {"repeatHold", true},
        {"repeatToggle", true}, {"repeatLongPress", true}, {"repeatSequence", true},
        {"repeatSequenceHotkey", true}, {"repeatPauseKeys", true},
        {"macroHotkey", true}, {"macroTriggerMode", true}, {"macroLoopCount", true},
        {"macroCurrentStep", true}, {"macroPauseKeys", true}};
    const auto workspaceChanged = [this]() {
        emit activityIconChanged(); emit overlayChanged();
        pluginContext_.setRuntimeState({{"driver", QStringList{tr("小叽"), QStringLiteral("DD"), QStringLiteral("USB")}[static_cast<int>(driverMode_)]},
            {"repeatRunning", keys_.model()->state().value("running")}, {"macroRunning", macros_.model()->state().value("running")}});
    };
    connect(keys_.model(), &PageModel::stateChanged, this, workspaceChanged);
    connect(macros_.model(), &PageModel::stateChanged, this, workspaceChanged);
    connect(&keys_, &PreviewWorkspaceController::overlaySettingsRequested, this, [this]() { showOverlaySettings(false); });
    connect(&macros_, &PreviewWorkspaceController::overlaySettingsRequested, this, [this]() { showOverlaySettings(true); });
    connect(&keys_, &PreviewWorkspaceController::soundSettingsRequested, this, [this] { showSoundSettings(false); });
    connect(&macros_, &PreviewWorkspaceController::soundSettingsRequested, this, [this] { showSoundSettings(true); });
    connect(&keys_, &PreviewWorkspaceController::aboutRequested, this, [this]() { navigate("about"); });
    connect(&macros_, &PreviewWorkspaceController::aboutRequested, this, [this]() { navigate("about"); });
    connect(&onboarding_, &PreviewOnboardingModel::navigateRequested, this, &PreviewController::navigate);
    connect(&settingsPage_, &SettingsPageModel::startupActionChanged, &settingsPage_, &SettingsPageModel::setStartupAction);
    connect(&settingsPage_, &SettingsPageModel::closeActionChanged, &settingsPage_, &SettingsPageModel::setCloseAction);
    connect(&settingsPage_, &SettingsPageModel::gameGateEnabledChanged, &settingsPage_, &SettingsPageModel::setGameGateEnabled);
    connect(&settingsPage_, &SettingsPageModel::autoStartToggled, &settingsPage_, &SettingsPageModel::setAutoStartEnabled);
    connect(&settingsPage_, &SettingsPageModel::automaticUpdateChecksToggled, &settingsPage_, &SettingsPageModel::setAutomaticUpdateChecks);
    connect(&pluginCenter_, &PluginCenterModel::actionRequested, this, &PreviewController::handlePluginAction);
    connect(&pluginCenter_, &PageModel::stateChanged, this, [this] {
        sidebar_.setPluginPages(pluginCenter_.state().value("pages").toList());
    });
    pluginCenter_.setPageContextProvider([this](const QString& id) -> QObject* {
        const auto descriptor = pluginCenter_.page(id);
        if (descriptor.isEmpty()) return nullptr;
        if (descriptor.value("pluginId").toString() == QStringLiteral("xiaoji.page.calendar")) return &calendarContext_;
        return descriptor.value("pluginId").toString() == QStringLiteral("example.desktop-pet") ? &petContext_ : &pluginContext_;
    });
    connect(&petContext_, &PluginPageContext::navigationRequested, this, &PreviewController::navigate);
    connect(this, &PreviewController::themeChanged, &petContext_, &PluginPageContext::refreshTheme);
    connect(&pluginContext_, &PluginPageContext::navigationRequested, this, &PreviewController::navigate);
    connect(&pluginContext_, &PluginPageContext::messageRequested, this, [this](const QString& title, const QString& message) {
        if (!dialog_.visible()) dialog_.message(title, message);
    });
    connect(&pluginContext_, &PluginPageContext::confirmationRequested, this, [this](const QString& id, const QString& title, const QString& message) {
        if (dialog_.visible()) { emit pluginContext_.confirmationFinished(id, false); return; }
        const auto answered = std::make_shared<bool>(false);
        auto* observer = new QObject(this);
        connect(&dialog_, &PreviewDialogModel::visibleChanged, observer, [this, observer, answered, id] {
            if (!dialog_.visible()) QTimer::singleShot(0, observer, [this, observer, answered, id] {
                if (!*answered) emit pluginContext_.confirmationFinished(id, false);
                observer->deleteLater();
            });
        });
        dialog_.message(title, message, [this, answered, id] { *answered = true; emit pluginContext_.confirmationFinished(id, true); });
    });
    connect(this, &PreviewController::themeChanged, &pluginContext_, &PluginPageContext::refreshTheme);
    connect(this, &PreviewController::themeChanged, &calendarContext_, &PluginPageContext::refreshTheme);
    connect(&driverPage_, &DriverPageModel::providerSelected, this, [this](const QString& id) {
        if (id.startsWith(QStringLiteral("xiaoji.driver.dd/"))) emit driverPage_.driverModeChanged(DriverMode::Legacy);
        else if (id.startsWith(QStringLiteral("xiaoji.driver.usb/"))) emit driverPage_.driverModeChanged(DriverMode::Usb);
        else emit driverPage_.driverModeChanged(DriverMode::Xiaoji);
        refreshPreviewPlugins();
    });
    connect(&driverPage_, &DriverPageModel::pluginCenterRequested, this, [this] { navigate(QStringLiteral("plugins")); });
    connect(&driverPage_, &DriverPageModel::pluginManagementRequested, this, [this](const QString& operation) {
        if (operation != QStringLiteral("uninstall")) return;
        const QStringList ids{QStringLiteral("xiaoji.driver.xiaoji"), QStringLiteral("xiaoji.driver.dd"), QStringLiteral("xiaoji.driver.usb")};
        handlePluginAction(QStringLiteral("uninstall-system"), ids[static_cast<int>(driverMode_)], {});
    });
    connect(&settingsPage_, &SettingsPageModel::factionChanged, this, &PreviewController::selectFaction);
    connect(&settingsPage_, &SettingsPageModel::gameGateProcessesEditRequested, this, &PreviewController::showProcesses);
    connect(&driverPage_, &DriverPageModel::driverModeChanged, this, [this](DriverMode mode) {
        if (mode == driverMode_) {
            if (mode == DriverMode::Xiaoji && !xiaojiInstalled_ && !xiaojiTrustEstablished_) {
                dialog_.present("DialogMessage.qml", tr("信任小叽本机签名证书"),
                    {{"message", tr("正式程序首次安装时，会单独确认本机签名证书信任，然后自动签名并安装内置驱动。\n\n"
                        "此处只预览确认后的界面状态，不操作证书、驱动或文件。")}},
                    [this](const QVariantMap&) {
                        xiaojiTrustEstablished_ = true;
                        xiaojiInstalled_ = true;
                        refreshDriver();
                    }, 460, true);
                return;
            }
            if (mode == DriverMode::Legacy) ddInstalled_ = true;
            if (mode == DriverMode::Xiaoji) xiaojiInstalled_ = true;
        }
        driverMode_ = mode;
        refreshDriver();
    });
    connect(&driverPage_, &DriverPageModel::uninstallDriverRequested, this, [this](DriverRemovalTarget mode) {
        dialog_.present("DialogMessage.qml", tr("卸载驱动"), {{"message", tr("确认展示卸载后的界面状态？此操作只修改预览数据。")}},
            [this, mode](const QVariantMap&) {
                if (mode == DriverRemovalTarget::Legacy) ddInstalled_ = false;
                if (mode == DriverRemovalTarget::Xiaoji) xiaojiInstalled_ = false;
                if (mode == DriverRemovalTarget::RetiredHid) hidInstalled_ = false;
                refreshDriver();
            }, 400, true);
    });
    connect(&driverPage_, &DriverPageModel::selfCheckRequested, this, &PreviewController::showDriverReport);
    connect(&driverPage_, &DriverPageModel::diagnosticsInfoRequested,
        this, &PreviewController::showDiagnosticReport);
    connect(&driverPage_, &DriverPageModel::refreshRequested, this, &PreviewController::refreshDriver);
    connect(&aboutPage_, &AboutPageModel::checkUpdateRequested, this, [this]() {
        aboutPage_.setChecking();
        QTimer::singleShot(400, this, [this]() {
            if (!demoData_) { aboutPage_.setNoUpdate(); return; }
            auto parts = version().split('.');
            parts.last() = QString::number(parts.last().toInt() + 1);
            aboutPage_.setUpdateAvailable(parts.join('.'), true);
            keys_.model()->update("updateVersion", parts.join('.'));
        });
    });
    connect(&aboutPage_, &AboutPageModel::updateActionRequested, this, &PreviewController::showUpdateProgress);
    connect(&aboutPage_, &AboutPageModel::usageGuideRequested, this, [this]() {
        dialog_.message(tr("使用说明"), tr("按键连发：配置按键与启停键，在右侧启动或停止。\n循环连发：配置按键序列、触发方式和循环次数。\n使用统计：查看近期每日时长，下方可按年份查看全年每月时长。\n\n当前为界面预览，配置操作仅修改内存中的示例数据。"));
    });
    connect(&aboutPage_, &AboutPageModel::onboardingRequested, this, [this]() {
        dialog_.resetEditorGuides();
        onboarding_.show();
    });
    connect(&shareBanner_, &ShareImportBannerModel::importRequested, this, [this](const QString&) { showExample(2); });
    setDemoData(true);
}

void PreviewController::attachWindow(QWindow* window) { window_ = window; }
QVariantMap PreviewController::theme() const { return Fluent::qmlTheme(); }
QString PreviewController::version() const { return QStringLiteral(APP_VERSION); }
QString PreviewController::title() const { return tr("小叽按键 · 界面预览"); }

QVariantList PreviewController::factions() const {
    QVariantList result;
    for (const auto& faction : FactionTheme::instance().factions()) {
        result.append(QVariantMap{{"id", faction.id}, {"name", faction.name},
            {"icon", QString(faction.iconPath).replace(":/", "qrc:/")}});
    }
    return result;
}

QString PreviewController::activityIcon() const {
    return keys_.model()->state().value("running").toBool() || macros_.model()->state().value("running").toBool()
        ? QStringLiteral("qrc:/assets/icon-active.png") : QStringLiteral("qrc:/assets/icon.png");
}

void PreviewController::setDarkMode(bool dark) {
    Fluent::Theme::darkModeCache() = dark;
    QGuiApplication::setPalette(Fluent::Theme::applicationPalette());
    emit themeChanged();
    emit overlayChanged();
    emit pickerChanged();
}

void PreviewController::setDemoData(bool enabled) {
    if (demoData_ == enabled) return;
    demoData_ = enabled;
    initializePreviewPlugins(enabled);
    keys_.reset(enabled);
    macros_.reset(enabled);
    std::map<std::string, std::int64_t> days;
    if (enabled) {
        const QDate today = QDate::currentDate();
        for (int offset = 0; offset < 400; ++offset) {
            if (offset % 7 == 2 || offset % 7 == 5) continue;
            const auto milliseconds = static_cast<std::int64_t>((offset * 17 + 12) % 240 + 1) * 60000;
            days.emplace(today.addDays(-offset).toString(Qt::ISODate).toStdString(), milliseconds);
        }
        days[today.toString(Qt::ISODate).toStdString()] = 92 * 60000;
    }
    usagePage_.setUsage(days, false);
    xiaojiInstalled_ = enabled;
    xiaojiTrustEstablished_ = enabled;
    ddInstalled_ = enabled;
    hidInstalled_ = enabled;
    refreshDriver();
    AppConfig settings;
    processes_ = enabled ? QStringList{QStringLiteral("mumu*")} : QStringList{};
    for (const auto& process : processes_) settings.gameGateProcesses.push_back(process.toStdString());
    settings.faction = FactionTheme::instance().current().id.toStdString();
    settingsPage_.setConfig(settings);
    aboutPage_.setUpdateIdle();
    onboarding_.skip();
    shareBanner_.hideBanner();
    emit demoDataChanged();
}

void PreviewController::navigate(const QString& page) {
    if (currentPage_ == page) return;
    currentPage_ = page;
    emit currentPageChanged();
}

void PreviewController::selectPage(const QString& page) {
    if (onboarding_.state().value("active").toBool()) return;
    if (QStringList{"keys", "macro", "usage", "driver", "settings", "about", "plugins"}.contains(page) || !pluginCenter_.page(page).isEmpty()) navigate(page);
}

void PreviewController::selectFaction(const QString& faction) {
    if (!FactionTheme::instance().find(faction)) return;
    FactionTheme::instance().setCurrent(faction);
    settingsPage_.setFaction(faction);
    emit themeChanged();
    emit overlayChanged();
}

void PreviewController::startWindowMove() { if (window_) window_->startSystemMove(); }
void PreviewController::minimizeWindow() { if (window_) window_->showMinimized(); }
void PreviewController::closeWindow() { if (window_) window_->close(); }

void PreviewController::refreshDriver() {
    const QString name = QStringList{tr("小叽"), "DD", "USB"}[static_cast<int>(driverMode_)];
    const bool ready = driverMode_ == DriverMode::Xiaoji ? xiaojiInstalled_
        : driverMode_ == DriverMode::Legacy ? ddInstalled_ : demoData_;
    driverPage_.setDriverMode(driverMode_);
    const auto providerId = QStringList{QStringLiteral("xiaoji.driver.xiaoji"), QStringLiteral("xiaoji.driver.dd"), QStringLiteral("xiaoji.driver.usb")}[static_cast<int>(driverMode_)];
    bool available = false;
    for (const auto& item : previewPlugins_) {
        const auto row = item.toMap();
        if (row.value("id").toString() == providerId && row.value("enabled").toBool()
            && !row.value("blocked").toBool() && !row.value("newInstall").toBool()) available = true;
    }
    driverPage_.setDriverActions(available && driverMode_ == DriverMode::Xiaoji && xiaojiInstalled_,
        available && driverMode_ == DriverMode::Legacy && ddInstalled_, hidInstalled_);
    driverPage_.setPluginManagement(false, available && ready && driverMode_ != DriverMode::Usb);
    driverPage_.setDiagnosticsActions(available, available);
    driverPage_.setInstallAction(available && !ready && driverMode_ != DriverMode::Usb, tr("安装 %1").arg(name));
    driverPage_.setStatusSummary(ready ? tr("%1 · 已就绪（示例）").arg(name) : tr("%1 · 未就绪（示例）").arg(name));
    QStringList lines{tr("配置驱动: %1").arg(name)};
    if (available && driverMode_ == DriverMode::Xiaoji) lines << QString() << tr("小叽驱动（默认）")
        << tr("键盘/鼠标: %1").arg(xiaojiInstalled_ ? tr("已就绪") : tr("未安装"))
        << tr("本机签名安装: 可用（示例）")
        << tr("输出范围: NKRO 键盘、媒体/浏览器键、鼠标五键、相对移动及滚轮");
    else if (available && driverMode_ == DriverMode::Legacy) lines << QString() << QStringLiteral("DD")
        << tr("服务/设备: %1").arg(ddInstalled_ ? tr("已就绪") : tr("未安装"));
    else if (available && driverMode_ == DriverMode::Usb) lines << QString() << QStringLiteral("USB")
        << tr("通信: %1").arg(demoData_ ? tr("已连接") : tr("未连接"));
    if (hidInstalled_) lines << QString() << tr("旧 HID: 检测到安装残留，仅提供卸载入口");
    driverPage_.setDetailText(lines.join(QLatin1Char('\n')));
}
