#pragma once

#include "AppTypes.h"

#include <filesystem>

class ConfigStore {
public:
    explicit ConfigStore(std::filesystem::path configPath);

    [[nodiscard]] AppConfig Load() const;
    bool Save(const AppConfig& config, std::wstring& error) const;
    bool SaveRawValidated(const std::string& content, std::wstring& error) const;
    [[nodiscard]] static bool IsValidConfigText(const std::string& content);
    [[nodiscard]] static bool IsValidConfigFile(const std::filesystem::path& path);

    [[nodiscard]] const std::filesystem::path& GetPath() const noexcept;
    void SetPath(std::filesystem::path newPath);

private:
    bool WriteAtomically(const std::string& content, std::wstring& error) const;
    std::filesystem::path configPath_;
};
