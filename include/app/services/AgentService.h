#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

struct AgentStatus {
    bool installed = false;
    bool taskRegistered = false;
    bool running = false;
    bool pausedForUi = false;
    bool usbConnected = false;
    bool firmwareUpdateRequired = false;
    std::wstring detail;
};

class AgentService {
public:
    static std::filesystem::path DataDirectory();
    static std::filesystem::path FrontendConfigPath();
    static bool PublishFrontendConfigPath(const std::filesystem::path& configPath,
        std::wstring* error = nullptr);
    static std::filesystem::path InstallDirectory();
    static std::filesystem::path ExecutablePath();

    static bool RecoverFrontendConfig(const std::filesystem::path& frontendConfig);
    static bool EnsureInstalled(const std::filesystem::path& sourceExe, std::wstring* error = nullptr);
    static int InstallElevated(const std::filesystem::path& sourceExe);
    static bool Uninstall(std::wstring* error = nullptr);
    static bool Start(std::wstring* error = nullptr);
    static bool IsRunning();
    static AgentStatus QueryStatus();

    static bool PauseForUi(std::uint32_t processId, bool* repeatRunning = nullptr,
        std::wstring* error = nullptr);
    static bool Resume(bool repeatRunning, std::wstring* error = nullptr);
    static bool Shutdown(std::wstring* error = nullptr);

    static std::wstring PipeName();

private:
    static std::filesystem::path LegacyConfigPath();
};
