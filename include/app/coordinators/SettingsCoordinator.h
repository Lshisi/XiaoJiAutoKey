#pragma once

#include "AppTypes.h"

#include <QObject>
#include <QString>
#include <functional>

class QMainWindow;
class SettingsPage;

class SettingsCoordinator : public QObject {
    Q_OBJECT

public:
    struct Context {
        SettingsPage* page = nullptr;
        AppConfig* config = nullptr;
        QMainWindow* window = nullptr;
        std::function<void()> saveConfig;
        std::function<void()> refreshFactionVisuals;
        std::function<void()> refreshOverlay;
        std::function<void()> editHotkey;
    };

    explicit SettingsCoordinator(QObject* parent = nullptr);

    void bind(Context context);
    void setFaction(const QString& factionId);

private:
    Context context_;
};
