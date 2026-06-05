#pragma once

#include "AppTypes.h"

#include <QObject>
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
    };

    explicit SettingsCoordinator(QObject* parent = nullptr);

    void bind(Context context);

private:
    Context context_;
};
