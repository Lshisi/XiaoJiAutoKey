#pragma once

#include <filesystem>
#include <string>

// Compatibility cleanup for installations that still contain the retired Agent.
class AgentService {
public:
    static bool RecoverFrontendConfig(const std::filesystem::path& frontendConfig);
    static bool Uninstall(std::wstring* error = nullptr);
    static std::wstring PipeName();

private:
    static std::filesystem::path DataDirectory();
    static std::filesystem::path InstallDirectory();
    static std::filesystem::path ExecutablePath();
    static std::filesystem::path LegacyConfigPath();
    static bool Shutdown(std::wstring* error);
};
