#pragma once

#include "InputDriver.h"

#include <Windows.h>

#include <mutex>
#include <string>
#include <unordered_map>

class Ch552UsbDriver final : public InputDriver {
public:
    Ch552UsbDriver() = default;
    ~Ch552UsbDriver() override;

    bool AutoConnect();
    bool Connect(const std::wstring& portName);
    void Disconnect();
    [[nodiscard]] static std::wstring DetectPortName();

    [[nodiscard]] bool IsReady() const noexcept override;
    [[nodiscard]] std::wstring GetLastError() const override;
    [[nodiscard]] std::wstring PortName() const;
    [[nodiscard]] std::wstring FirmwareVersion() const;
    bool EnterBootloaderForUpdate();

    bool SendTap(const KeyDescriptor& key, InputSource src = InputSource::Injected) override;
    bool SendDown(const KeyDescriptor& key, InputSource src = InputSource::Injected) override;
    bool SendUp(const KeyDescriptor& key, InputSource src = InputSource::Injected) override;
    void ReleaseAll() override;
    void ClearAllStates() override;

private:
    bool OpenPortLocked(const std::wstring& portName);
    void ClosePortLocked();
    bool WriteLineLocked(const std::string& line);
    std::string ReadAvailableLocked(DWORD waitMs);
    void DrainFastResponsesLocked();
    bool QueryVersionLocked();
    bool SendCommandLocked(const std::string& line);
    bool SendFastCommandLocked(const std::string& line);
    static std::wstring FormatWin32Error(DWORD errorCode);
    static std::wstring NormalizePortName(const std::wstring& portName);

    mutable std::mutex mutex_;
    HANDLE handle_ = INVALID_HANDLE_VALUE;
    std::wstring portName_;
    std::wstring firmwareVersion_;
    std::wstring lastError_;
    std::string fastResponseBuffer_;
    bool fastInputSupported_ = false;
    std::unordered_map<std::uint64_t, int> pressedKeys_;
};
