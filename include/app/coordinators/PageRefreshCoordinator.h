#pragma once

#include "AppTypes.h"

#include <QObject>
#include <cstdint>
#include <filesystem>
#include <functional>

class MacroEngine;
class MainWindowView;
class RepeaterEngine;

class PageRefreshCoordinator : public QObject {
    Q_OBJECT

public:
    struct Context {
        MainWindowView* view = nullptr;
        AppConfig* config = nullptr;
        RepeaterEngine* repeaterEngine = nullptr;
        MacroEngine* macroEngine = nullptr;
        std::filesystem::path configPath;
        std::function<ProfileConfig*()> activeProfile;
        std::function<MacroConfig*()> activeMacro;
        std::function<void()> refreshFactionVisuals;
        std::function<void()> refreshDriverPage;
    };

    explicit PageRefreshCoordinator(QObject* parent = nullptr);

    void bind(Context context);
    void refreshAll();
    void refreshProfiles();
    void refreshMacroProfiles();
    void refreshMacro();

private:
    Context context_;
};
