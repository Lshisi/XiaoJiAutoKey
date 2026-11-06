#pragma once

#include "AppTypes.h"

#include <QObject>
#include <QVariantMap>
#include <cstdint>
#include <functional>
#include <optional>

class KeyCaptureDialogBase;
class GlobalInputHook;
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
        GlobalInputHook* inputHook = nullptr;
    };

    explicit KeyBindingsCoordinator(QObject* parent = nullptr);

    void bind(Context context);
    void editFromOverview(const QVariantMap& target, QWidget* parent);

private:
    void onAddBinding();
    void onEditBinding(std::uint64_t id);
    void onDeleteBinding(std::uint64_t id);
    void onEnableToggled(std::uint64_t id, bool enabled);
    void onBatchEdit(const QVariantList& bindingIds);
    void onBatchDelete(const QVariantList& bindingIds);
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
    void editPauseKey(std::optional<int> index);
    void onPauseKeyDelete(int index);
    void openEditDialog(std::optional<std::uint64_t> bindingId, std::optional<KeyDescriptor> initialKey = std::nullopt);
    std::optional<std::size_t> findBindingIndex(std::uint64_t id) const;

    Context context_;
};
