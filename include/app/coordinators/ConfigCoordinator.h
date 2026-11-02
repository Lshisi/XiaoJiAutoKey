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
        std::function<void()> refreshHookState;
        std::function<void()> saved;
        std::function<void()> beforeSave;
    };

    explicit ConfigCoordinator(std::filesystem::path configPath, QObject* parent = nullptr);

    void bind(Context context);
    void load();
    bool save(bool showError = true, std::uint32_t lockTimeoutMs = 30000);

    [[nodiscard]] const std::filesystem::path& configPath() const noexcept;

private:
    ConfigStore store_;
    Context context_;
};
