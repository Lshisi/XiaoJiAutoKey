#include "PreviewController.h"

#include "FactionTheme.h"
#include "ui/styles/QmlTheme.h"
#include <QDate>
#include <QGuiApplication>
#include <QTimer>
#include <QWindow>

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
    const auto workspaceChanged = [this]() { emit activityIconChanged(); emit overlayChanged(); };
    connect(keys_.model(), &PageModel::stateChanged, this, workspaceChanged);
    connect(macros_.model(), &PageModel::stateChanged, this, workspaceChanged);
    connect(&keys_, &PreviewWorkspaceController::overlaySettingsRequested, this, [this]() { showOverlaySettings(false); });
    connect(&macros_, &PreviewWorkspaceController::overlaySettingsRequested, this, [this]() { showOverlaySettings(true); });
    connect(&keys_, &PreviewWorkspaceController::aboutRequested, this, [this]() { navigate("about"); });
    connect(&macros_, &PreviewWorkspaceController::aboutRequested, this, [this]() { navigate("about"); });
    connect(&onboarding_, &PreviewOnboardingModel::navigateRequested, this, &PreviewController::navigate);
    connect(&settingsPage_, &SettingsPageModel::startupActionChanged, &settingsPage_, &SettingsPageModel::setStartupAction);
    connect(&settingsPage_, &SettingsPageModel::closeActionChanged, &settingsPage_, &SettingsPageModel::setCloseAction);
    connect(&settingsPage_, &SettingsPageModel::gameGateEnabledChanged, &settingsPage_, &SettingsPageModel::setGameGateEnabled);
    connect(&settingsPage_, &SettingsPageModel::autoStartToggled, &settingsPage_, &SettingsPageModel::setAutoStartEnabled);
    connect(&settingsPage_, &SettingsPageModel::factionChanged, this, &PreviewController::selectFaction);
    connect(&settingsPage_, &SettingsPageModel::gameGateProcessesEditRequested, this, &PreviewController::showProcesses);
    connect(&driverPage_, &DriverPageModel::driverModeChanged, this, [this](DriverMode mode) {
        if (mode == driverMode_) {
            if (mode == DriverMode::Legacy) ddInstalled_ = true;
            if (mode == DriverMode::Hid) hidInstalled_ = true;
        }
        driverMode_ = mode;
        refreshDriver();
    });
    connect(&driverPage_, &DriverPageModel::uninstallDriverRequested, this, [this](DriverMode mode) {
        dialog_.present("DialogMessage.qml", tr("卸载驱动"), {{"message", tr("确认展示卸载后的界面状态？此操作只修改预览数据。")}},
            [this, mode](const QVariantMap&) {
                if (mode == DriverMode::Legacy) ddInstalled_ = false;
                if (mode == DriverMode::Hid) hidInstalled_ = false;
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
    ddInstalled_ = enabled;
    hidInstalled_ = enabled;
    refreshDriver();
    AppConfig settings;
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
    if (QStringList{"keys", "macro", "usage", "driver", "settings", "about"}.contains(page)) navigate(page);
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
    const QString name = QStringList{"DD", "HID", "USB"}[static_cast<int>(driverMode_)];
    const bool ready = driverMode_ == DriverMode::Legacy ? ddInstalled_
        : driverMode_ == DriverMode::Hid ? hidInstalled_ : demoData_;
    driverPage_.setDriverMode(driverMode_);
    driverPage_.setDriverActions(ddInstalled_, hidInstalled_);
    driverPage_.setInstallAction(!ready && driverMode_ != DriverMode::Usb, tr("安装 %1").arg(name));
    driverPage_.setStatusSummary(ready ? tr("%1 · 已就绪（示例）").arg(name) : tr("%1 · 未就绪（示例）").arg(name));
    driverPage_.setDetailText(tr("配置驱动: %1\n当前错误: %2\nDD32.dll: 已找到\nDD64.dll: 已找到\nINF 键盘服务: %3\nINF 鼠标服务: %3\nINF 键盘设备: %4\nINF 鼠标设备: %4\nHID DLL: 已找到\nHID 服务: %5\nHID SYS: %5\nHID Root 设备: %6\nHID 子设备: %6\nUSB 设备: 示例设备\nUSB 通信: %7\nUSB 固件: 界面预览")
        .arg(name, ready ? tr("无（示例状态）") : tr("示例驱动未安装"),
            ddInstalled_ ? tr("已安装") : tr("未安装"), ddInstalled_ ? QStringLiteral("OK") : tr("未安装"),
            hidInstalled_ ? tr("已安装") : tr("未安装"), hidInstalled_ ? QStringLiteral("OK") : tr("未安装"),
            demoData_ ? tr("已连接") : tr("未连接")));
}
