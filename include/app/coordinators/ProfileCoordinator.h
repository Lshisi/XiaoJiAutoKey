#pragma once

#include "AppTypes.h"

#include <QObject>
#include <cstdint>
#include <functional>

class GlobalInputHook;
class KeyPage;
class MacroController;
class MacroEngine;
class ProfileController;
class RepeaterEngine;
class QWidget;

class ProfileCoordinator : public QObject {
    Q_OBJECT

public:
    struct Context {
        QWidget* parent = nullptr;
        KeyPage* page = nullptr;
        AppConfig* config = nullptr;
        ProfileController* profileController = nullptr;
        MacroController* macroController = nullptr;
        RepeaterEngine* repeaterEngine = nullptr;
        MacroEngine* macroEngine = nullptr;
        GlobalInputHook* inputHook = nullptr;
        std::function<void(bool)> setRepeatRunning;
        std::function<void()> applyBindings;
        std::function<void()> applyMacros;
        std::function<void()> saveConfig;
        std::function<void()> refreshAll;
        std::function<void()> refreshProfileCombo;
    };

    explicit ProfileCoordinator(QObject* parent = nullptr);

    void bind(Context context);
    void switchToProfile(std::uint64_t profileId);

private:
    void onProfileSwitched(std::uint64_t profileId);
    void onProfileAdd();
    void onProfileRename();
    void onProfileDelete();
    void onProfileEnabledToggled(bool enabled);
    void stopEnginesForProfileChange();

    Context context_;
};
