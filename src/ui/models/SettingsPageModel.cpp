#include "SettingsPageModel.h"

#include <QDateTime>
#include <QGuiApplication>

SettingsPageModel::SettingsPageModel(QObject* parent) : PageModel(parent) {
    state_ = {{"hotkey", tr("未设置")}, {"startupAction", 1}, {"closeAction", 0},
        {"gameGateEnabled", false}, {"gameGateProcessCount", 0},
        {"autoStartEnabled", false}, {"automaticUpdateChecks", true}, {"faction", QStringLiteral("cangjian")},
        {"crispyThemeUnlocked", false}, {"crispyThemeEnabled", false}, {"crispyThemeReceiptVisible", false}};
    calendarTimer_.setSingleShot(true);
    connect(&calendarTimer_, &QTimer::timeout, this, &SettingsPageModel::refreshCalendar);
    connect(qGuiApp, &QGuiApplication::applicationStateChanged, this, [this](Qt::ApplicationState state) {
        if (state == Qt::ApplicationActive) refreshCalendar();
    });
    refreshCalendar();
}

void SettingsPageModel::setConfig(const AppConfig& config) {
    state_.insert("startupAction", static_cast<int>(config.startupAction));
    state_.insert("closeAction", static_cast<int>(config.closeAction));
    state_.insert("gameGateEnabled", config.gameGateEnabled);
    state_.insert("gameGateProcessCount", static_cast<int>(config.gameGateProcesses.size()));
    state_.insert("autoStartEnabled", config.autoStartEnabled);
    state_.insert("automaticUpdateChecks", config.automaticUpdateChecks);
    state_.insert("crispyThemeUnlocked", config.crispyThemeUnlocked);
    state_.insert("crispyThemeEnabled", config.crispyThemeEnabled);
    setFaction(QString::fromStdString(config.faction));
}

void SettingsPageModel::setFaction(const QString& id) {
    if (state_.value("faction").toString() != id) {
        emblemClickCount_ = 0;
        emblemClickTimer_.invalidate();
    }
    setValue("faction", id);
}

void SettingsPageModel::setCrispyThemeState(bool unlocked, bool enabled) {
    state_.insert("crispyThemeUnlocked", unlocked);
    setValue("crispyThemeEnabled", enabled);
}

void SettingsPageModel::refreshCalendar() {
    const auto now = QDateTime::currentDateTime();
    const bool thursday = now.date().dayOfWeek() == Qt::Thursday;
    if (state_.value("thursday").toBool() != thursday || !state_.contains("thursday"))
        setValue("thursday", thursday);
    if (!thursday) {
        emblemClickCount_ = 0;
        emblemClickTimer_.invalidate();
    }
    auto midnight = now.addDays(1);
    midnight.setTime(QTime(0, 0));
    calendarTimer_.start(static_cast<int>(now.msecsTo(midnight)));
}

void SettingsPageModel::activateFactionEmblem() {
    if (state_.value("faction").toString() != QStringLiteral("cangjian")) return;
    if (state_.value("crispyThemeUnlocked").toBool()) {
        if (state_.value("crispyThemeEnabled").toBool()) showCrispyThemeReceipt();
        return;
    }
    refreshCalendar();
    if (!state_.value("thursday").toBool()) return;
    if (!emblemClickTimer_.isValid() || emblemClickTimer_.elapsed() > 3000) {
        emblemClickTimer_.start();
        emblemClickCount_ = 0;
    }
    if (++emblemClickCount_ < 5) return;
    emblemClickCount_ = 0;
    emblemClickTimer_.invalidate();
    emit crispyThemeUnlockRequested();
    showCrispyThemeReceipt();
}

void SettingsPageModel::setCrispyThemeReceiptVisible(bool visible) {
    if (state_.value("crispyThemeReceiptVisible").toBool() == visible) return;
    setValue("crispyThemeReceiptVisible", visible);
    emit crispyThemeReceiptVisibleChanged(visible);
}

void SettingsPageModel::showCrispyThemeReceipt() {
    if (state_.value("crispyThemeUnlocked").toBool()) setCrispyThemeReceiptVisible(true);
}

void SettingsPageModel::dismissCrispyThemeReceipt() { setCrispyThemeReceiptVisible(false); }

void SettingsPageModel::selectCrispyTheme() {
    if (!state_.value("crispyThemeUnlocked").toBool()) return;
    emit crispyThemeSelected();
    dismissCrispyThemeReceipt();
}

void SettingsPageModel::selectStartupAction(int value) {
    if (value < 0 || value > 1) return;
    emit startupActionChanged(static_cast<StartupAction>(value));
}

void SettingsPageModel::selectCloseAction(int value) {
    if (value < 0 || value > 2) return;
    emit closeActionChanged(static_cast<CloseAction>(value));
}
