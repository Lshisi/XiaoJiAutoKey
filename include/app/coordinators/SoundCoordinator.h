#pragma once

#include "AppTypes.h"

#include <QObject>
#include <cstdint>
#include <functional>

class GlobalInputHook;
class KeyPage;
class MacroPage;
class SoundService;
class QWidget;

class SoundCoordinator : public QObject {
    Q_OBJECT

public:
    struct Context {
        QWidget* parent = nullptr;
        KeyPage* keyPage = nullptr;
        MacroPage* macroPage = nullptr;
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
