#pragma once

#include <filesystem>
#include <functional>
#include <string>
#include <vector>
#include <utility>

class DdRuntimeService {
public:
    struct RuntimeStatus {
        bool driverReady = false;
        std::wstring lastError;
        bool driverRootExists = false;
        std::wstring driverRootPath;
        bool xiaojiManagerExists = false;
        bool xiaojiInstallerExists = false;
        bool xiaojiKeyboardPresent = false;
        bool xiaojiMousePresent = false;
        bool xiaojiKeyboardReady = false;
        bool xiaojiMouseReady = false;
        bool xiaojiProfileReady = false;
        bool xiaojiBoundPackagePresent = false;
        bool xiaojiConfigurationPresent = false;
        std::wstring xiaojiError;
        // Legacy HID detection exists only to keep its uninstall entry available.
        bool hidDevicesPresent = false;
        bool hidServiceInstalled = false;
        bool hidServicePendingDelete = false;
        bool hidSysInstalled = false;
        bool hidDriverStorePresent = false;
        bool hidRootDeviceOk = false;
        bool hidChildDeviceOk = false;
        bool legacy32Exists = false;
        bool legacy64Exists = false;
        bool legacyRuntimeExists = false;
        std::wstring legacyRuntimePath;
        bool embeddedInfVariant0Exists = false;
        bool embeddedInfVariant1Exists = false;
        bool infKeyboardServiceInstalled = false;
        bool infMouseServiceInstalled = false;
        bool infKeyboardServicePendingDelete = false;
        bool infMouseServicePendingDelete = false;
        bool infKeyboardSysInstalled = false;
        bool infMouseSysInstalled = false;
        bool infKeyboardDeviceStarted = false;
        bool infMouseDeviceStarted = false;
        bool infKeyboardDeviceBound = false;
        bool infMouseDeviceBound = false;
        std::vector<std::wstring> candidateDlls;
        std::vector<std::wstring> notes;
    };

    explicit DdRuntimeService(std::filesystem::path exeDir, std::filesystem::path logsDir = {});

    [[nodiscard]] RuntimeStatus Inspect(bool driverReady, const std::wstring& lastError) const;
    [[nodiscard]] bool UninstallEmbeddedHidDriver(std::wstring* resultMessage = nullptr) const;
    [[nodiscard]] bool InstallEmbeddedInfDriver(std::wstring* resultMessage = nullptr) const;
    [[nodiscard]] bool UninstallEmbeddedInfDriver(std::wstring* resultMessage = nullptr, bool* restartRequired = nullptr) const;
    [[nodiscard]] bool RepairEmbeddedInfDriver(const std::function<bool()>& readinessProbe,
                                               std::wstring* resultMessage = nullptr) const;
    void LogRuntimeError(const wchar_t* fileName, const std::wstring& message) const;
    [[nodiscard]] bool InfDevicesReady() const;
    [[nodiscard]] std::filesystem::path ExeDir() const noexcept;
    [[nodiscard]] std::filesystem::path LogsDir() const noexcept;
    [[nodiscard]] std::filesystem::path DriverRootDir() const;
    void SetXiaojiDirectory(std::filesystem::path directory) { xiaojiDirectory_ = std::move(directory); }

private:
    [[nodiscard]] bool EmbeddedInfVariantExists(int variant) const;
    [[nodiscard]] bool HidRootDeviceOk() const;
    [[nodiscard]] bool HidChildDeviceOk() const;
    [[nodiscard]] bool DeviceInstanceOk(const wchar_t* instanceIdPrefix) const;
    [[nodiscard]] bool ServiceExists(const wchar_t* serviceName) const;
    [[nodiscard]] std::filesystem::path SystemDriversDir() const;
    [[nodiscard]] bool HidDriverStorePresent() const;
    [[nodiscard]] std::filesystem::path Legacy32Path() const;
    [[nodiscard]] std::filesystem::path Legacy64Path() const;

    std::filesystem::path exeDir_;
    std::filesystem::path logsDir_;
    std::filesystem::path xiaojiDirectory_;
};
