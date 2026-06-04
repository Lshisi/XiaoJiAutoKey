#pragma once

#include "AppTypes.h"

#include <Windows.h>

#include <atomic>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>

class DdDriver {
public:
    DdDriver() = default;
    ~DdDriver();

    bool LoadFrom(const std::wstring& dllPath);
    bool LoadFromDirectory(const std::wstring& directoryPath);
    void Unload();

    [[nodiscard]] bool IsReady() const noexcept;
    [[nodiscard]] std::wstring GetLastError() const;
    [[nodiscard]] bool IsSimulating() const noexcept;
    [[nodiscard]] bool IsKeySimulating(const KeyDescriptor& key) const;

    bool SendTap(const KeyDescriptor& key, InputSource src = InputSource::Injected);
    bool SendDown(const KeyDescriptor& key, InputSource src = InputSource::Injected);
    bool SendUp(const KeyDescriptor& key, InputSource src = InputSource::Injected);
    void ReleaseAll();
    void ClearAllStates();  // 清空所有内部状态（pressedKeys_和selfInjected_）
    bool IsSelfInjected(const KeyDescriptor& key) const;

private:
    using DD_btn_t = int(__stdcall*)(int);
    using DD_key_t = int(__stdcall*)(int, int);
    using DD_todc_t = int(__stdcall*)(int);

    bool SendKeyboardTap(std::uint32_t vk);
    bool SendMouseTap(std::uint32_t vk);
    void RegisterPress(const KeyDescriptor& key);
    void RegisterRelease(const KeyDescriptor& key);
    void MarkInjected(const KeyDescriptor& key);

    HMODULE module_ = nullptr;
    DD_btn_t ddBtn_ = nullptr;
    DD_key_t ddKey_ = nullptr;
    DD_todc_t ddToDc_ = nullptr;
    std::wstring lastError_;
    std::atomic<int> simulateDepth_{0};

    mutable     std::mutex mutex_;
    std::unordered_map<std::uint64_t, int> pressedKeys_;
    std::unordered_set<std::uint64_t> selfInjected_;
};
