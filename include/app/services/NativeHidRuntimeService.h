#pragma once

#include <filesystem>
#include <string>

class NativeHidRuntimeService {
public:
    struct RuntimeStatus {
        bool packageExists = false;
        bool serviceInstalled = false;
        bool sysInstalled = false;
        bool devicePresent = false;
        bool deviceStarted = false;
        bool interfacePresent = false;
        std::wstring publishedInfName;
    };

    NativeHidRuntimeService(std::filesystem::path exeDir, std::filesystem::path logsDir);

    [[nodiscard]] RuntimeStatus Inspect() const;
    [[nodiscard]] bool Install(std::wstring* resultMessage = nullptr) const;
    [[nodiscard]] bool Uninstall(std::wstring* resultMessage = nullptr) const;
    [[nodiscard]] std::filesystem::path PackageDir() const;
    [[nodiscard]] std::filesystem::path InfPath() const;

private:
    std::filesystem::path exeDir_;
    std::filesystem::path logsDir_;
};

