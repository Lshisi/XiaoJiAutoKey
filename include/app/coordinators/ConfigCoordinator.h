#pragma once

#include "AppTypes.h"
#include "ConfigStore.h"

#include <QObject>

#include <cstdint>
#include <filesystem>
#include <functional>

class QWidget;

class ConfigCoordinator : public QObject {
    Q_OBJECT

public:
    struct Context {
        QWidget* parent = nullptr;
        AppConfig* config = nullptr;
        std::uint64_t* activeMacroId = nullptr;
        std::function<void()> refreshHookState;
    };

    explicit ConfigCoordinator(std::filesystem::path configPath, QObject* parent = nullptr);

    void bind(Context context);
    void load();
    void save();

    [[nodiscard]] const std::filesystem::path& configPath() const noexcept;

private:
    ConfigStore store_;
    Context context_;
};
