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
struct RepeaterStatus;

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
        std::function<void()> saveConfig;
    };

    explicit SoundCoordinator(QObject* parent = nullptr);

    void bind(Context context);
    void playEvent(SoundEvent event) const;
    void playToggleSound(bool running, bool macroMode) const;
    void handleRepeatInput(const RepeaterStatus& before, const RepeaterStatus& after) const;

private:
    void openSettings(SoundGroup group);

    Context context_;
};
