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
        Legacy,
    };

    DdDriver() = default;
    ~DdDriver() override;

    bool LoadFrom(const std::wstring& dllPath);
    bool LoadLegacyFromDirectory(const std::wstring& directoryPath);
    void Unload();
    void SwapWith(DdDriver& other);

    [[nodiscard]] bool IsReady() const noexcept override;
    [[nodiscard]] std::wstring GetLastError() const override;
    [[nodiscard]] RuntimeKind GetRuntimeKind() const noexcept;
    [[nodiscard]] bool IsSimulating() const noexcept;
    [[nodiscard]] bool IsKeySimulating(const KeyDescriptor& key) const;

    bool SendTap(const KeyDescriptor& key, InputSource src = InputSource::Injected) override;
    bool SendDown(const KeyDescriptor& key, InputSource src = InputSource::Injected) override;
    bool RestorePhysicalDown(const KeyDescriptor& key, InputSource src = InputSource::Injected) override;
    bool SendUp(const KeyDescriptor& key, InputSource src = InputSource::Injected) override;
    bool SendMouseMove(int deltaX, int deltaY, InputSource src = InputSource::Injected) override;
    [[nodiscard]] bool SupportsMouseMove() const override;
    bool SendMouseWheel(int delta, InputSource src = InputSource::Injected) override;
    [[nodiscard]] bool SupportsMouseWheel() const override;
    void ReleaseAll() override;
    void ClearAllStates() override;  // Clear pressedKeys_ and selfInjected_ state.
    bool ConsumeSelfInjected(const KeyDescriptor& key, bool pressed) override;
    bool IsSelfInjected(const KeyDescriptor& key) const;

private:
    using DD_btn_t = int(__stdcall*)(int);
    using DD_key_t = int(__stdcall*)(int, int);
    using DD_todc_t = int(__stdcall*)(int);
    using DD_move_t = int(__stdcall*)(int, int);

    bool SendDownInternal(const KeyDescriptor& key, InputSource src, bool trackHold);
    bool SendKeyboardTap(std::uint32_t vk);
    bool SendMouseTap(std::uint32_t vk);
    bool SendMouseDown(std::uint32_t vk);
    bool SendMouseUp(std::uint32_t vk);
    // 鼠标注入结果诊断：连续失败只在首次记录（调用方需持有 ioMutex_）。
    void NoteMouseInjectResult(const KeyDescriptor& key, bool down, bool ok);
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
    DD_btn_t ddWheel_ = nullptr;
    DD_key_t ddKey_ = nullptr;
    DD_todc_t ddToDc_ = nullptr;
    // 可选导出：DD_mov(绝对)/DD_movR(相对)，老版 DD DLL 可能没有。
    DD_move_t ddMov_ = nullptr;
    DD_move_t ddMovR_ = nullptr;
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

    std::unordered_set<std::uint64_t> mouseInjectFailDiag_;  // ioMutex_ 保护：连续注入失败只记录首次
};
