#include "coordinators/SettingsCoordinator.h"

#include "FactionTheme.h"
#include "ui/pages/SettingsPage.h"

SettingsCoordinator::SettingsCoordinator(QObject* parent) : QObject(parent) {}

void SettingsCoordinator::bind(Context context) {
    context_ = std::move(context);
    if (!context_.page || !context_.config) return;

    connect(context_.page, &SettingsPage::hotkeyEditClicked, this, [this]() {
        if (context_.editHotkey) context_.editHotkey();
    });
    connect(context_.page, &SettingsPage::startupActionChanged, this, [this](StartupAction action) {
        context_.config->startupAction = action;
        if (context_.saveConfig) context_.saveConfig();
    });
    connect(context_.page, &SettingsPage::closeActionChanged, this, [this](CloseAction action) {
        context_.config->closeAction = action;
        if (context_.saveConfig) context_.saveConfig();
    });
    connect(context_.page, &SettingsPage::statusOverlayOpacityChanged, this, [this](int opacity) {
        context_.config->statusOverlayOpacity = opacity;
        if (context_.saveConfig) context_.saveConfig();
        if (context_.refreshOverlay) context_.refreshOverlay();
    });
    connect(context_.page, &SettingsPage::factionChanged, this, [this](const QString& factionId) {
        setFaction(factionId);
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
