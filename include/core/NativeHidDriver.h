#pragma once

#include "InputDriver.h"

#include <Windows.h>

#include <atomic>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>

class NativeHidDriver final : public InputDriver {
public:
    NativeHidDriver() = default;
    ~NativeHidDriver() override;

    bool Connect();
    void Disconnect();
    [[nodiscard]] static bool DevicePresent();

    [[nodiscard]] bool IsReady() const noexcept override;
    [[nodiscard]] std::wstring GetLastError() const override;
    [[nodiscard]] std::uint32_t DriverVersion() const noexcept;

    bool SendTap(const KeyDescriptor& key, InputSource src = InputSource::Injected) override;
    bool SendDown(const KeyDescriptor& key, InputSource src = InputSource::Injected) override;
    bool SendUp(const KeyDescriptor& key, InputSource src = InputSource::Injected) override;
    void ReleaseAll() override;
    void ClearAllStates() override;
    bool ConsumeSelfInjected(const KeyDescriptor& key, bool pressed) override;

private:
    struct InjectedState {
        int downCount = 0;
        int upCount = 0;
        ULONGLONG lastDownTick = 0;
        ULONGLONG lastUpTick = 0;
    };

    static std::wstring FindDevicePath();
    static std::optional<std::uint16_t> ToKeyboardUsage(std::uint32_t vk);
    static std::optional<std::uint16_t> ToConsumerUsage(std::uint32_t vk);
    static std::optional<std::uint8_t> ToMouseButton(std::uint32_t vk);
    static std::uint64_t EncodeKey(const KeyDescriptor& key) noexcept;
    static std::wstring FormatWin32Error(DWORD errorCode);

    bool SendKeyboardLocked(std::uint16_t usage, bool pressed);
    bool SendConsumerLocked(std::uint16_t usage, bool pressed);
    bool SendMouseButtonLocked(std::uint8_t button, bool pressed);
    bool SendIoctlLocked(DWORD ioctl, const void* input = nullptr, DWORD inputSize = 0,
                         void* output = nullptr, DWORD outputSize = 0);
    void HeartbeatLoop(std::stop_token stopToken);
    void MarkInjectedLocked(const KeyDescriptor& key, bool pressed);

    mutable std::mutex mutex_;
    HANDLE handle_ = INVALID_HANDLE_VALUE;
    bool leaseOwned_ = false;
    std::jthread heartbeatThread_;
    std::wstring lastError_;
    std::uint32_t driverVersion_ = 0;
    std::unordered_map<std::uint64_t, int> pressedKeys_;
    std::unordered_map<std::uint64_t, InjectedState> selfInjected_;
};
