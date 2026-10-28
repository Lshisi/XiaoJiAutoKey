#pragma once

#include "AppTypes.h"
#include "controllers/ControllerResult.h"

#include <QObject>
#include <functional>

class GlobalInputHook;
class KeyCaptureDialogBase;
class KeyPageModel;
class MacroController;
class MacroPageModel;
class ProfileController;
class RawInputManager;
class SettingsPageModel;
class QWidget;

class CaptureCoordinator : public QObject {
    Q_OBJECT

public:
    enum class Target { MainHotkey, SequenceSwitchKey, DialogKey, MacroHotkey };

    struct Context {
        QWidget* parent = nullptr;
        KeyPageModel* keyPage = nullptr;
        MacroPageModel* macroPage = nullptr;
        SettingsPageModel* settingsPage = nullptr;
        ProfileController* profileController = nullptr;
        MacroController* macroController = nullptr;
        GlobalInputHook* inputHook = nullptr;
        RawInputManager* rawInput = nullptr;
        std::function<void()> applyBindings;
        std::function<void()> applyMacros;
        std::function<void()> saveConfig;
        std::function<void()> refreshAll;
        std::function<void()> refreshMacroPage;
        std::function<void()> refreshHookState;
        std::function<ControllerResult(const KeyDescriptor&)> validateHotkeyCandidate;
    };

    explicit CaptureCoordinator(QObject* parent = nullptr);

    void bind(Context context);
    void startMainHotkeyCapture();
    void startSequenceSwitchKeyCapture();
    void startMacroHotkeyCapture();
    void runKeyCaptureDialog(KeyCaptureDialogBase& dialog);
    void handleCapturedKey(quint64 encodedKey);
    void cancelCapture();

    [[nodiscard]] bool captureActive() const;

private:
    void updateHotkeyText();
    void handleDecodedKey(const KeyDescriptor& key);

    Context context_;
    std::optional<Target> target_;
    KeyCaptureDialogBase* activeDialog_ = nullptr;
};
