#pragma once

#include "InputDriver.h"

#include <Windows.h>

#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

class PicoUsbDriver final : public InputDriver {
public:
    PicoUsbDriver() = default;
    ~PicoUsbDriver() override;

    bool AutoConnect();
    bool Connect(const std::wstring& portName);
    void Disconnect();

    [[nodiscard]] bool IsReady() const noexcept override;
    [[nodiscard]] std::wstring GetLastError() const override;
    [[nodiscard]] std::wstring PortName() const;
    [[nodiscard]] std::wstring FirmwareVersion() const;

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
    bool QueryVersionLocked();
    bool SendCommandLocked(const std::string& line);
    static std::wstring FormatWin32Error(DWORD errorCode);
    static std::wstring NormalizePortName(const std::wstring& portName);

    mutable std::mutex mutex_;
    HANDLE handle_ = INVALID_HANDLE_VALUE;
    std::wstring portName_;
    std::wstring firmwareVersion_;
    std::wstring lastError_;
    std::unordered_map<std::uint64_t, int> pressedKeys_;
};
