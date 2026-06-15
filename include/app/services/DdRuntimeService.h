#pragma once

#include "DdDriver.h"

#include <filesystem>
#include <string>
#include <vector>

class DdRuntimeService {
public:
    struct RuntimeStatus {
        bool driverReady = false;
        std::wstring lastError;
        bool legacy32Exists = false;
        bool legacy64Exists = false;
        bool embeddedInfVariant0Exists = false;
        bool embeddedInfVariant1Exists = false;
        bool infKeyboardServiceInstalled = false;
        bool infMouseServiceInstalled = false;
        bool infKeyboardSysInstalled = false;
        bool infMouseSysInstalled = false;
        std::filesystem::path logFilePath;
        std::vector<std::wstring> candidateDlls;
        std::vector<std::wstring> notes;
    };

    DdRuntimeService(std::filesystem::path exeDir, std::filesystem::path logsDir);

    [[nodiscard]] RuntimeStatus Inspect(const DdDriver& driver) const;
    [[nodiscard]] bool InstallEmbeddedInfDriver(std::wstring* resultMessage = nullptr) const;
    [[nodiscard]] std::filesystem::path ExeDir() const noexcept;
    [[nodiscard]] std::filesystem::path LogsDir() const noexcept;
    [[nodiscard]] std::filesystem::path DriverRootDir() const;

private:
    [[nodiscard]] bool EmbeddedInfVariantExists(int variant) const;
    [[nodiscard]] bool ServiceExists(const wchar_t* serviceName) const;
    [[nodiscard]] std::filesystem::path SystemDriversDir() const;
    [[nodiscard]] std::filesystem::path Legacy32Path() const;
    [[nodiscard]] std::filesystem::path Legacy64Path() const;

    std::filesystem::path exeDir_;
    std::filesystem::path logsDir_;
};
