#include "coordinators/SettingsCoordinator.h"

#include "FactionTheme.h"
#include "ui/pages/SettingsPage.h"

#include <QDesktopServices>
#include <QUrl>

SettingsCoordinator::SettingsCoordinator(QObject* parent) : QObject(parent) {}

void SettingsCoordinator::bind(Context context) {
    context_ = std::move(context);
    if (!context_.page || !context_.config) return;

    connect(context_.page, &SettingsPage::hotkeyEditClicked, this, [this]() {
        if (context_.editHotkey) context_.editHotkey();
    });
    connect(context_.page, &SettingsPage::openConfigFolder, this, [this]() {
        QDesktopServices::openUrl(QUrl::fromLocalFile(QString::fromStdWString(context_.configPath.parent_path().wstring())));
    });
    connect(context_.page, &SettingsPage::closeActionChanged, this, [this](CloseAction action) {
        context_.config->closeAction = action;
        if (context_.saveConfig) context_.saveConfig();
    });
    connect(context_.page, &SettingsPage::factionChanged, this, [this](const QString& factionId) {
        context_.config->faction = factionId.toStdString();
        FactionTheme::instance().setCurrent(factionId);
        if (context_.saveConfig) context_.saveConfig();
        if (context_.refreshFactionVisuals) context_.refreshFactionVisuals();
    });
}
