#pragma once

#include "AppTypes.h"

#include <QObject>
#include <functional>

class GlobalInputHook;
class KeyCaptureDialogBase;
class KeyPage;
class MacroController;
class MacroPage;
class ProfileController;
class SettingsPage;
class QWidget;

class CaptureCoordinator : public QObject {
    Q_OBJECT

public:
    enum class Target { MainHotkey, DialogKey, MacroHotkey };

    struct Context {
        QWidget* parent = nullptr;
        KeyPage* keyPage = nullptr;
        MacroPage* macroPage = nullptr;
        SettingsPage* settingsPage = nullptr;
        ProfileController* profileController = nullptr;
        MacroController* macroController = nullptr;
        GlobalInputHook* inputHook = nullptr;
        std::function<void()> applyMacros;
        std::function<void()> saveConfig;
        std::function<void()> refreshAll;
        std::function<void()> refreshMacroPage;
        std::function<void()> refreshHookState;
    };

    explicit CaptureCoordinator(QObject* parent = nullptr);

    void bind(Context context);
    void startMainHotkeyCapture();
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
