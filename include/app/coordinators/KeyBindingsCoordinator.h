#pragma once

#include "AppTypes.h"

#include <QObject>
#include <cstdint>
#include <functional>
#include <optional>

class KeyCaptureDialogBase;
class KeyPageModel;
class ProfileController;
class RepeaterEngine;
class QWidget;

class KeyBindingsCoordinator : public QObject {
    Q_OBJECT

public:
    struct Context {
        QWidget* parent = nullptr;
        KeyPageModel* page = nullptr;
        ProfileController* controller = nullptr;
        RepeaterEngine* engine = nullptr;
        std::function<void(KeyCaptureDialogBase&)> runKeyCaptureDialog;
        std::function<void()> startHotkeyCapture;
        std::function<void()> startSequenceSwitchKeyCapture;
        std::function<void(bool)> setRepeatRunning;
        std::function<void()> applyBindings;
        std::function<void()> saveConfig;
        std::function<void()> refreshAll;
        std::function<void(bool)> setBeepEnabled;
        std::function<void(bool)> setStatusOverlayEnabled;
        std::function<MacroConfig*()> activeMacro;
        std::function<bool()> canUseDdDriver;
        std::function<void()> showDdUnavailableMessage;
        std::function<bool()> driverSupportsMouseMove;
    };

    explicit KeyBindingsCoordinator(QObject* parent = nullptr);

    void bind(Context context);

private:
    void onAddBinding();
    void onEditBinding(std::uint64_t id);
    void onDeleteBinding(std::uint64_t id);
    void onEnableToggled(std::uint64_t id, bool enabled);
    void onStartClicked();
    void onHotkeyEditClicked();
    void onHotkeyClearClicked();
    void onBeepToggled(bool enabled);
    void onStatusOverlayToggled(bool enabled);
    void onMixedModeToggled(bool enabled);
    void onWasdProtectionToggled(bool enabled);
    void onRememberSequencePositionChanged(bool enabled);
    void onSequenceSwitchKeyAdd();
    void onSequenceSwitchKeyDelete();
    void onPauseKeyAdd();
    void onPauseKeyDelete(int index);
    void openEditDialog(std::optional<std::uint64_t> bindingId);
    std::optional<std::size_t> findBindingIndex(std::uint64_t id) const;

    Context context_;
};
