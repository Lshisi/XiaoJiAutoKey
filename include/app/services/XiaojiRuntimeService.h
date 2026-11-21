#pragma once

#include <filesystem>
#include <string>

class XiaojiRuntimeService {
public:
    struct Status {
        bool managerExists = false;
        bool installerExists = false;
        bool keyboardPresent = false;
        bool mousePresent = false;
        bool keyboardReady = false;
        bool mouseReady = false;
        bool profileReady = false;
        bool boundPackagePresent = false;
        bool configurationPresent = false;
        std::wstring error;
    };

    explicit XiaojiRuntimeService(std::filesystem::path exeDir);
    [[nodiscard]] Status Inspect() const;
    [[nodiscard]] std::filesystem::path ManagerPath() const;
    [[nodiscard]] std::filesystem::path InstallerPath() const;
    [[nodiscard]] std::filesystem::path BundledInfPath() const;
    bool Install(const std::filesystem::path& signedInf, std::wstring* message) const;
    bool ReadTrustToken(std::wstring* token, std::wstring* message) const;
    bool InstallLocal(const std::wstring& approvedToken, std::wstring* message) const;
    bool Repair(std::wstring* message) const;
    bool Uninstall(std::wstring* message) const;

private:
    bool Run(const std::wstring& command, const std::filesystem::path& argument,
        std::wstring* message) const;
    std::filesystem::path exeDir_;
};
