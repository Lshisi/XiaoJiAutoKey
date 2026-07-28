#pragma once

#include "InputDriver.h"

#include <Windows.h>

#include <atomic>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class DdDriver : public InputDriver {
public:
    enum class RuntimeKind {
        Unknown,
        Hid,
        Legacy,
    };

    DdDriver() = default;
    ~DdDriver() override;

    bool LoadFrom(const std::wstring& dllPath);
    bool LoadHidFrom(const std::wstring& dllPath);
    bool LoadFromDirectory(const std::wstring& directoryPath, bool allowHid = true);
    void Unload();
    void SwapWith(DdDriver& other);

    [[nodiscard]] bool IsReady() const noexcept override;
    [[nodiscard]] std::wstring GetLastError() const override;
    [[nodiscard]] RuntimeKind GetRuntimeKind() const noexcept;
    [[nodiscard]] bool IsSimulating() const noexcept;
    [[nodiscard]] bool IsKeySimulating(const KeyDescriptor& key) const;

    bool SendTap(const KeyDescriptor& key, InputSource src = InputSource::Injected) override;
    bool SendDown(const KeyDescriptor& key, InputSource src = InputSource::Injected) override;
    bool SendUp(const KeyDescriptor& key, InputSource src = InputSource::Injected) override;
    void ReleaseAll() override;
    void ClearAllStates() override;  // Clear pressedKeys_ and selfInjected_ state.
    bool ConsumeSelfInjected(const KeyDescriptor& key, bool pressed) override;
    bool IsSelfInjected(const KeyDescriptor& key) const;

private:
    using DD_btn_t = int(__stdcall*)(int);
    using DD_key_t = int(__stdcall*)(int, int);
    using DD_todc_t = int(__stdcall*)(int);

    bool LoadFrom(const std::wstring& dllPath, RuntimeKind runtimeKind);
    bool SendKeyboardTap(std::uint32_t vk);
    bool SendMouseTap(std::uint32_t vk);
    bool SendMouseDown(std::uint32_t vk);
    bool SendMouseUp(std::uint32_t vk);
    bool SendHidMouseButton(std::uint32_t vk, bool down);
    bool EnsureHidMouseHandleLocked();
    bool WriteHidMouseReportLocked();
    void CloseHidMouseHandleLocked();
    bool TryBindInfDeviceHandles(const std::wstring& dllPath, std::wstring* errorMessage);
    bool WriteModuleHandleSlot(std::uintptr_t rva, HANDLE value);
    void CloseInjectedInfHandles();
    void RegisterPress(const KeyDescriptor& key);
    void RegisterRelease(const KeyDescriptor& key);
    void MarkInjected(const KeyDescriptor& key, bool pressed, int eventCount = 1);
    void RollbackInjected(const KeyDescriptor& key, bool pressed, int eventCount = 1);

    struct InjectedState {
        int downCount = 0;
        int upCount = 0;
        ULONGLONG lastDownTick = 0;
        ULONGLONG lastUpTick = 0;
    };

    HMODULE module_ = nullptr;
    DD_btn_t ddBtn_ = nullptr;
    DD_key_t ddKey_ = nullptr;
    DD_todc_t ddToDc_ = nullptr;
    RuntimeKind runtimeKind_ = RuntimeKind::Unknown;
    HANDLE infKeyboardHandle_ = INVALID_HANDLE_VALUE;
    HANDLE infMouseHandle_ = INVALID_HANDLE_VALUE;
    bool infHandlesInjected_ = false;
    std::wstring lastError_;
    std::atomic<int> simulateDepth_{0};

    std::mutex ioMutex_;
    mutable std::mutex mutex_;
    std::unordered_map<std::uint64_t, int> pressedKeys_;
    std::unordered_map<std::uint64_t, InjectedState> selfInjected_;

    std::mutex hidMouseMutex_;
    HANDLE hidMouseHandle_ = INVALID_HANDLE_VALUE;
    std::uint8_t hidMouseButtonState_ = 0;
};
