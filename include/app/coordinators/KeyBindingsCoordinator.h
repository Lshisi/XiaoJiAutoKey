#pragma once

#include "AppTypes.h"

#include <QObject>
#include <cstdint>
#include <functional>
#include <optional>

class KeyCaptureDialogBase;
class KeyPage;
class ProfileController;
class RepeaterEngine;
class QWidget;

class KeyBindingsCoordinator : public QObject {
    Q_OBJECT

public:
    struct Context {
        QWidget* parent = nullptr;
        KeyPage* page = nullptr;
        ProfileController* controller = nullptr;
        RepeaterEngine* engine = nullptr;
        std::function<void(KeyCaptureDialogBase&)> runKeyCaptureDialog;
        std::function<void()> startHotkeyCapture;
        std::function<void(bool)> setRepeatRunning;
        std::function<void()> applyBindings;
        std::function<void()> saveConfig;
        std::function<void()> refreshAll;
        std::function<void(bool)> setBeepEnabled;
        std::function<MacroConfig*()> activeMacro;
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
    void onBeepToggled(bool enabled);
    void onMixedModeToggled(bool enabled);
    void onPauseKeyAdd();
    void onPauseKeyDelete(int index);
    void openEditDialog(std::optional<std::uint64_t> bindingId);
    std::optional<std::size_t> findBindingIndex(std::uint64_t id) const;

    Context context_;
};
