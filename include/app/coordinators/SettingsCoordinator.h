#pragma once

#include "AppTypes.h"

#include <QObject>
#include <QString>
#include <filesystem>
#include <functional>

class QMainWindow;
class SettingsPage;

class SettingsCoordinator : public QObject {
    Q_OBJECT

public:
    struct Context {
        SettingsPage* page = nullptr;
        AppConfig* config = nullptr;
        std::filesystem::path configPath;
        QMainWindow* window = nullptr;
        std::function<void()> saveConfig;
        std::function<void()> refreshFactionVisuals;
        std::function<void()> refreshOverlay;
        std::function<void()> editHotkey;
        std::function<void(DriverMode)> changeDriverMode;
        std::function<void()> refreshDriverStatus;
        std::function<void()> uninstallHidDriver;
    };

    explicit SettingsCoordinator(QObject* parent = nullptr);

    void bind(Context context);
    void setFaction(const QString& factionId);

private:
    Context context_;
};
