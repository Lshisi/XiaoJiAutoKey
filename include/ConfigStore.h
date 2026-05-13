#pragma once

#include "AppTypes.h"

#include <filesystem>

class ConfigStore {
public:
    explicit ConfigStore(std::filesystem::path configPath);

    [[nodiscard]] AppConfig Load() const;
    bool Save(const AppConfig& config, std::wstring& error) const;

    [[nodiscard]] const std::filesystem::path& GetPath() const noexcept;

private:
    std::filesystem::path configPath_;
};
