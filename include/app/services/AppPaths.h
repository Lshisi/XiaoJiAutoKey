#pragma once

#include <filesystem>
#include <string>

class AppPaths {
public:
    AppPaths();

    [[nodiscard]] const std::wstring& exeDir() const noexcept;
    [[nodiscard]] const std::filesystem::path& configPath() const noexcept;

private:
    std::wstring exeDir_;
    std::filesystem::path configPath_;
};
