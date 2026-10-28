#pragma once

#include "AppTypes.h"

#include <QObject>
#include <QString>
#include <functional>

class QMainWindow;
class SettingsPageModel;

class SettingsCoordinator : public QObject {
    Q_OBJECT

public:
    struct Context {
        SettingsPageModel* page = nullptr;
        AppConfig* config = nullptr;
        QMainWindow* window = nullptr;
        std::function<void()> saveConfig;
        std::function<void()> refreshFactionVisuals;
        std::function<void()> editHotkey;
        std::function<void()> applyGameGate;
        std::function<void()> applyAutoStart;
    };

    explicit SettingsCoordinator(QObject* parent = nullptr);

    void bind(Context context);
    void setFaction(const QString& factionId);

private:
    Context context_;
};
