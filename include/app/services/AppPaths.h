#pragma once

#include <filesystem>
#include <string>

class AppPaths {
public:
    AppPaths();

    [[nodiscard]] const std::wstring& exeDir() const noexcept;
    [[nodiscard]] const std::filesystem::path& configPath() const noexcept;
    [[nodiscard]] const std::filesystem::path& logsDir() const noexcept;

private:
    std::wstring exeDir_;
    std::filesystem::path configPath_;
    std::filesystem::path logsDir_;
};
