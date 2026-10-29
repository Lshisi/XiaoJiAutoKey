#include "coordinators/SettingsCoordinator.h"

#include "FactionTheme.h"
#include "dialogs/GameProcessDialog.h"
#include "ui/models/SettingsPageModel.h"

#include <QMainWindow>
#include <QStringList>

SettingsCoordinator::SettingsCoordinator(QObject* parent) : QObject(parent) {}

void SettingsCoordinator::bind(Context context) {
    context_ = std::move(context);
    if (!context_.page || !context_.config) return;

    connect(context_.page, &SettingsPageModel::hotkeyEditClicked, this, [this]() {
        if (context_.editHotkey) context_.editHotkey();
    });
    connect(context_.page, &SettingsPageModel::startupActionChanged, this, [this](StartupAction action) {
        context_.config->startupAction = action;
        context_.page->setStartupAction(action);
        if (context_.saveConfig) context_.saveConfig();
    });
    connect(context_.page, &SettingsPageModel::closeActionChanged, this, [this](CloseAction action) {
        context_.config->closeAction = action;
        context_.page->setCloseAction(action);
        if (context_.saveConfig) context_.saveConfig();
    });
    connect(context_.page, &SettingsPageModel::factionChanged, this, [this](const QString& factionId) {
        setFaction(factionId);
    });
    connect(context_.page, &SettingsPageModel::gameGateEnabledChanged, this, [this](bool enabled) {
        context_.config->gameGateEnabled = enabled;
        context_.page->setGameGateEnabled(enabled);
        if (context_.saveConfig) context_.saveConfig();
        if (context_.applyGameGate) context_.applyGameGate();
    });
    connect(context_.page, &SettingsPageModel::autoStartToggled, this, [this](bool enabled) {
        context_.config->autoStartEnabled = enabled;
        context_.page->setAutoStartEnabled(enabled);
        if (context_.saveConfig) context_.saveConfig();
        if (context_.applyAutoStart) context_.applyAutoStart();
    });
    connect(context_.page, &SettingsPageModel::gameGateProcessesEditRequested, this, [this]() {
        QStringList initial;
        for (const auto& name : context_.config->gameGateProcesses) {
            initial.push_back(QString::fromStdString(name));
        }
        GameProcessDialog dialog(initial, context_.window);
        if (dialog.exec() != QDialog::Accepted) {
            return;
        }
        std::vector<std::string> updated;
        for (const auto& name : dialog.processes()) {
            updated.push_back(name.toStdString());
        }
        context_.config->gameGateProcesses = std::move(updated);
        if (context_.saveConfig) context_.saveConfig();
        if (context_.applyGameGate) context_.applyGameGate();
    });
}

void SettingsCoordinator::setFaction(const QString& factionId) {
    if (!context_.config || factionId.isEmpty()) {
        return;
    }

    context_.config->faction = factionId.toStdString();
    FactionTheme::instance().setCurrent(factionId);
    if (context_.saveConfig) context_.saveConfig();
    if (context_.refreshFactionVisuals) context_.refreshFactionVisuals();
}
