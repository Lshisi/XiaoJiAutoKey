#include "services/AppPaths.h"

#include <shlwapi.h>
#include <windows.h>

namespace {

std::wstring ResolveExeDir() {
    std::wstring path(MAX_PATH, L'\0');
    const DWORD length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
    path.resize(length);
    PathRemoveFileSpecW(path.data());
    path.resize(wcslen(path.c_str()));
    return path;
}

}  // namespace

AppPaths::AppPaths()
    : exeDir_(ResolveExeDir()),
      configPath_(std::filesystem::path(exeDir_) / L"config.json"),
      logsDir_(std::filesystem::path(exeDir_) / L"logs") {}

const std::wstring& AppPaths::exeDir() const noexcept {
    return exeDir_;
}

const std::filesystem::path& AppPaths::configPath() const noexcept {
    return configPath_;
}

const std::filesystem::path& AppPaths::logsDir() const noexcept {
    return logsDir_;
}
