#pragma once

#include "AppTypes.h"

#include <QObject>
#include <cstdint>
#include <functional>

class GlobalInputHook;
class KeyPageModel;
class MacroPageModel;
class SoundService;
class QWidget;

class SoundCoordinator : public QObject {
    Q_OBJECT

public:
    struct Context {
        QWidget* parent = nullptr;
        KeyPageModel* keyPage = nullptr;
        MacroPageModel* macroPage = nullptr;
        AppConfig* config = nullptr;
        SoundService* service = nullptr;
        GlobalInputHook* inputHook = nullptr;
        std::function<bool()> macroModeActive;
        std::function<void()> saveConfig;
    };

    explicit SoundCoordinator(QObject* parent = nullptr);

    void bind(Context context);
    void playToggleSound(bool running, bool macroMode) const;

private:
    void openSettings();

    Context context_;
    mutable std::uint64_t lastToggleSoundTick_ = 0;
};
