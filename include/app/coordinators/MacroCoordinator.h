#pragma once

#include "AppTypes.h"
#include "controllers/ControllerResult.h"

#include <QObject>
#include <QVariantMap>
#include <cstdint>
#include <functional>

class GlobalInputHook;
class MacroController;
class MacroEngine;
class MacroPageModel;
class QWidget;
class KeyCaptureDialogBase;

class MacroCoordinator : public QObject {
    Q_OBJECT

public:
    struct Context {
        QWidget* parent = nullptr;
        MacroPageModel* page = nullptr;
        MacroController* controller = nullptr;
        MacroEngine* engine = nullptr;
        GlobalInputHook* inputHook = nullptr;
        std::function<void(KeyCaptureDialogBase&)> runKeyCaptureDialog;
        std::function<void()> applyMacros;
        std::function<void()> saveConfig;
        std::function<void()> refreshMacroPage;
        std::function<void()> refreshMacroProfileCombo;
        std::function<void()> refreshStatusIndicators;
        std::function<void(bool)> setRepeatRunning;
        std::function<void(bool)> playToggleSound;
        std::function<void(bool)> setMacroBeepEnabled;
        std::function<void(bool)> setStatusOverlayEnabled;
        std::function<ProfileConfig*()> activeProfile;
        std::function<ControllerResult()> validateActiveConfig;
        std::function<bool()> canUseDdDriver;
        std::function<void()> showDdUnavailableMessage;
        std::function<bool()> driverSupportsMouseMove;
    };

    explicit MacroCoordinator(QObject* parent = nullptr);

    void bind(Context context);
    void editFromOverview(const QVariantMap& target, QWidget* parent);
    void pollRunningState();

private:
    void onMacroSwitched(std::uint64_t macroId);
    void onMacroAdd();
    void onMacroDuplicate();
    void onMacroRename();
    void onMacroDelete();
    void onMacroEnabledToggled(bool enabled);
    void onMacroAddStep();
    void onRecordMacro();
    void onMacroInsertStep(const QString& stepId, bool before);
    void onMacroEditStep(const QString& stepId);
    void onMacroDeleteStep(const QString& stepId);
    void onStepEnabledToggled(const QString& stepId, bool enabled);
    void onMacroDuplicateStep(const QString& stepId);
    void onMacroBatchDuplicate(const QVariantList& stepIds);
    void onMacroBatchDelete(const QVariantList& stepIds);
    void onMacroBatchEdit(const QVariantList& stepIds);
    void onMacroMoveStep(const QString& stepId, int targetIndex);
    void onMacroStartStopClicked();
    void onMacroHotkeyClearClicked();
    void onMacroModeChanged(int modeIndex);
    void onMacroLoopCountChanged(int count);
    void onMacroBeepToggled(bool enabled);
    void onStatusOverlayToggled(bool enabled);
    void onPauseKeyAdd();
    void editPauseKey(std::optional<int> index);
    void onPauseKeyDelete(int index);
    void stopAllForConfigChange();
    void addStep(int insertIndex, bool focusCreated, std::optional<MacroStep> initial = std::nullopt);

    Context context_;
    bool lastMacroRunning_ = false;
};
