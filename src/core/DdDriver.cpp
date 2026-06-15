#include "DdDriver.h"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>

namespace {

std::uint64_t EncodeKey(const KeyDescriptor& key) {
    return (static_cast<std::uint64_t>(key.device == InputDevice::Mouse ? 1 : 0) << 32) | key.code;
}

std::optional<std::pair<int, int>> ToMouseCodes(std::uint32_t vk) {
    switch (vk) {
    case VK_LBUTTON:
        return std::make_pair(1, 2);
    case VK_RBUTTON:
        return std::make_pair(4, 8);
    case VK_MBUTTON:
        return std::make_pair(16, 32);
    case VK_XBUTTON1:
        return std::make_pair(64, 128);
    case VK_XBUTTON2:
        return std::make_pair(256, 512);
    default:
        return std::nullopt;
    }
}

using DDToDcFn = int(__stdcall*)(int);

constexpr const wchar_t* kInfKeyboardDevicePath = L"\\\\.\\dd.key.96688";
constexpr const wchar_t* kInfMouseDevicePath = L"\\\\.\\dd.mou.96688";
constexpr std::uintptr_t kDd2018KeyboardHandleSlotRva = 0x2120c8;
constexpr std::uintptr_t kDd2018MouseHandleSlotRva = 0x2129d0;

std::wstring FormatWin32Error(DWORD errorCode) {
    if (errorCode == ERROR_SUCCESS) {
        return L"0";
    }

    wchar_t* buffer = nullptr;
    const DWORD length = ::FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        errorCode,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<LPWSTR>(&buffer),
        0,
        nullptr);

    std::wstringstream text;
    text << errorCode;
    if (length > 0 && buffer != nullptr) {
        text << L" (" << buffer << L")";
    }
    if (buffer != nullptr) {
        ::LocalFree(buffer);
    }
    return text.str();
}

bool BinaryFileContainsWideText(const std::wstring& filePath, const wchar_t* text) {
    std::ifstream file(std::filesystem::path(filePath), std::ios::binary);
    if (!file) {
        return false;
    }

    const std::string bytes((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    const auto* textBytes = reinterpret_cast<const char*>(text);
    const std::size_t textByteCount = std::wcslen(text) * sizeof(wchar_t);
    if (textByteCount == 0 || bytes.size() < textByteCount) {
        return false;
    }
    return std::search(bytes.begin(), bytes.end(), textBytes, textBytes + textByteCount) != bytes.end();
}

bool IsInfBackedDdDll(const std::wstring& dllPath) {
    return BinaryFileContainsWideText(dllPath, kInfKeyboardDevicePath) &&
           BinaryFileContainsWideText(dllPath, kInfMouseDevicePath);
}

std::optional<int> ModifierDdCode(std::uint32_t vk) {
    switch (vk) {
    case VK_SHIFT:
    case VK_LSHIFT:
        return 500;
    case VK_RSHIFT:
        return 511;
    case VK_CONTROL:
    case VK_LCONTROL:
        return 600;
    case VK_RCONTROL:
        return 607;
    case VK_LWIN:
        return 601;
    case VK_RWIN:
        return 605;
    case VK_MENU:
    case VK_LMENU:
        return 602;
    case VK_RMENU:
        return 604;
    default:
        return std::nullopt;
    }
}

std::optional<int> NumpadDdCode(std::uint32_t vk) {
    if (vk >= VK_NUMPAD0 && vk <= VK_NUMPAD9) {
        return 800 + static_cast<int>(vk - VK_NUMPAD0);
    }

    switch (vk) {
    case VK_NUMLOCK:
        return 810;
    case VK_DIVIDE:
        return 811;
    case VK_MULTIPLY:
        return 812;
    case VK_SUBTRACT:
        return 813;
    case VK_ADD:
        return 814;
    case kNumpadEnterKeyCode:
        return 815;
    case VK_DECIMAL:
        return 816;
    default:
        return std::nullopt;
    }
}

int ResolveKeyboardDdCode(DDToDcFn ddToDc, std::uint32_t vk) {
    if (const auto modifierCode = ModifierDdCode(vk); modifierCode.has_value()) {
        return *modifierCode;
    }
    if (const auto numpadCode = NumpadDdCode(vk); numpadCode.has_value()) {
        return *numpadCode;
    }

    return ddToDc(static_cast<int>(vk));
}

}  // namespace

DdDriver::~DdDriver() {
    ReleaseAll();
    Unload();
}

bool DdDriver::LoadFrom(const std::wstring& dllPath) {
    Unload();

    module_ = ::LoadLibraryW(dllPath.c_str());
    if (module_ == nullptr) {
        const DWORD errorCode = ::GetLastError();
        lastError_ = L"Unable to load DD DLL: " + dllPath + L"; win32=" + FormatWin32Error(errorCode);
        return false;
    }

    ddBtn_ = reinterpret_cast<DD_btn_t>(::GetProcAddress(module_, "DD_btn"));
    ddKey_ = reinterpret_cast<DD_key_t>(::GetProcAddress(module_, "DD_key"));
    ddToDc_ = reinterpret_cast<DD_todc_t>(::GetProcAddress(module_, "DD_todc"));
    if (ddBtn_ == nullptr || ddKey_ == nullptr || ddToDc_ == nullptr) {
        lastError_ = L"DD DLL is missing required exports";
        Unload();
        return false;
    }

    int initResult = ddBtn_(0);
    if (initResult != 1) {
        std::wstring bindError;
        if (TryBindInfDeviceHandles(dllPath, &bindError)) {
            initResult = ddBtn_(0);
        }
        if (initResult != 1) {
            lastError_ = L"Please install the DD INF driver package first; DD_btn(0)=" + std::to_wstring(initResult);
            if (!bindError.empty()) {
                lastError_ += L"; INF bind=" + bindError;
            }
            Unload();
            return false;
        }
    }

    const int keyProbeCode = ResolveKeyboardDdCode(ddToDc_, 'A');
    if (keyProbeCode <= 0) {
        lastError_ = L"DD runtime loaded, but DD_todc probe failed.";
        Unload();
        return false;
    }

    lastError_.clear();
    return true;
}

bool DdDriver::LoadFromDirectory(const std::wstring& directoryPath) {
    const std::filesystem::path directory(directoryPath);
    const std::filesystem::path driverDir = directory / L"driver";

#if defined(_WIN64)
    const std::vector<std::filesystem::path> candidates = {
        driverDir / L"DD64.dll",
        directory / L"DD64.dll",
    };
#else
    const std::vector<std::filesystem::path> candidates = {
        driverDir / L"DD32.dll",
        directory / L"DD32.dll",
    };
#endif

    std::wstring firstMeaningfulError;
    for (const auto& candidate : candidates) {
        if (LoadFrom(candidate.wstring())) {
            return true;
        }
        if (firstMeaningfulError.empty() ||
            (firstMeaningfulError.rfind(L"Unable to load DD DLL:", 0) == 0 &&
             lastError_.rfind(L"Unable to load DD DLL:", 0) != 0)) {
            firstMeaningfulError = lastError_;
        }
    }

    if (!firstMeaningfulError.empty()) {
        lastError_ = firstMeaningfulError;
    } else {
#if defined(_WIN64)
        lastError_ = L"Unable to load DD64.dll";
#else
        lastError_ = L"Unable to load DD32.dll";
#endif
    }
    return false;
}

void DdDriver::Unload() {
    CloseInjectedInfHandles();
    if (module_ != nullptr) {
        ::FreeLibrary(module_);
        module_ = nullptr;
    }
    ddBtn_ = nullptr;
    ddKey_ = nullptr;
    ddToDc_ = nullptr;
}

bool DdDriver::TryBindInfDeviceHandles(const std::wstring& dllPath, std::wstring* errorMessage) {
    if (module_ == nullptr) {
        if (errorMessage) {
            *errorMessage = L"module is not loaded";
        }
        return false;
    }
    if (!IsInfBackedDdDll(dllPath)) {
        if (errorMessage) {
            *errorMessage = L"runtime DLL is not an INF-backed DD build";
        }
        return false;
    }

    HANDLE keyboardHandle = CreateFileW(
        kInfKeyboardDevicePath,
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (keyboardHandle == INVALID_HANDLE_VALUE) {
        if (errorMessage) {
            *errorMessage = L"open " + std::wstring(kInfKeyboardDevicePath) + L" failed: " + FormatWin32Error(::GetLastError());
        }
        return false;
    }

    HANDLE mouseHandle = CreateFileW(
        kInfMouseDevicePath,
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (mouseHandle == INVALID_HANDLE_VALUE) {
        const DWORD errorCode = ::GetLastError();
        CloseHandle(keyboardHandle);
        if (errorMessage) {
            *errorMessage = L"open " + std::wstring(kInfMouseDevicePath) + L" failed: " + FormatWin32Error(errorCode);
        }
        return false;
    }

    if (!WriteModuleHandleSlot(kDd2018KeyboardHandleSlotRva, keyboardHandle)) {
        const DWORD errorCode = ::GetLastError();
        CloseHandle(mouseHandle);
        CloseHandle(keyboardHandle);
        if (errorMessage) {
            *errorMessage = L"write keyboard handle slot failed: " + FormatWin32Error(errorCode);
        }
        return false;
    }

    if (!WriteModuleHandleSlot(kDd2018MouseHandleSlotRva, mouseHandle)) {
        const DWORD errorCode = ::GetLastError();
        WriteModuleHandleSlot(kDd2018KeyboardHandleSlotRva, INVALID_HANDLE_VALUE);
        CloseHandle(mouseHandle);
        CloseHandle(keyboardHandle);
        if (errorMessage) {
            *errorMessage = L"write mouse handle slot failed: " + FormatWin32Error(errorCode);
        }
        return false;
    }

    CloseInjectedInfHandles();
    infKeyboardHandle_ = keyboardHandle;
    infMouseHandle_ = mouseHandle;
    infHandlesInjected_ = true;
    if (errorMessage) {
        *errorMessage = L"bound " + dllPath + L" to INF devices";
    }
    return true;
}

bool DdDriver::WriteModuleHandleSlot(std::uintptr_t rva, HANDLE value) {
    auto* slot = reinterpret_cast<HANDLE*>(reinterpret_cast<std::uintptr_t>(module_) + rva);
    DWORD oldProtect = 0;
    if (!VirtualProtect(slot, sizeof(HANDLE), PAGE_READWRITE, &oldProtect)) {
        return false;
    }
    *slot = value;
    DWORD ignored = 0;
    const BOOL restored = VirtualProtect(slot, sizeof(HANDLE), oldProtect, &ignored);
    FlushInstructionCache(GetCurrentProcess(), slot, sizeof(HANDLE));
    return restored != FALSE;
}

void DdDriver::CloseInjectedInfHandles() {
    if (infHandlesInjected_ && module_ != nullptr) {
        WriteModuleHandleSlot(kDd2018KeyboardHandleSlotRva, INVALID_HANDLE_VALUE);
        WriteModuleHandleSlot(kDd2018MouseHandleSlotRva, INVALID_HANDLE_VALUE);
    }
    if (infKeyboardHandle_ != INVALID_HANDLE_VALUE) {
        CloseHandle(infKeyboardHandle_);
        infKeyboardHandle_ = INVALID_HANDLE_VALUE;
    }
    if (infMouseHandle_ != INVALID_HANDLE_VALUE) {
        CloseHandle(infMouseHandle_);
        infMouseHandle_ = INVALID_HANDLE_VALUE;
    }
    infHandlesInjected_ = false;
}

bool DdDriver::IsReady() const noexcept {
    return module_ != nullptr && ddBtn_ != nullptr && ddKey_ != nullptr && ddToDc_ != nullptr;
}

std::wstring DdDriver::GetLastError() const {
    return lastError_;
}

bool DdDriver::IsSimulating() const noexcept {
    return simulateDepth_.load() > 0;
}

bool DdDriver::IsKeySimulating(const KeyDescriptor& key) const {
    std::lock_guard lock(mutex_);
    const auto it = pressedKeys_.find(EncodeKey(key));
    return it != pressedKeys_.end() && it->second > 0;
}

bool DdDriver::SendTap(const KeyDescriptor& key, InputSource src) {
    if (!IsReady()) return false;
    if (simulateDepth_.load() > 3) return false;

    if (src == InputSource::Injected) {
        MarkInjected(key);
    }

    return key.device == InputDevice::Mouse ? SendMouseTap(key.code) : SendKeyboardTap(key.code);
}

bool DdDriver::SendDown(const KeyDescriptor& key, InputSource src) {
    if (!IsReady()) return false;
    if (simulateDepth_.load() > 3) return false;

    if (src == InputSource::Injected) {
        MarkInjected(key);
    }

    if (key.device == InputDevice::Mouse) {
        const auto mouseCodes = ToMouseCodes(key.code);
        if (!mouseCodes.has_value()) return false;
        ++simulateDepth_;
        RegisterPress(key);
        const int result = ddBtn_(mouseCodes->first);
        --simulateDepth_;
        return result == 1;
    }

    const int ddCode = ResolveKeyboardDdCode(ddToDc_, key.code);
    if (ddCode <= 0) return false;
    ++simulateDepth_;
    RegisterPress(key);
    const int result = ddKey_(ddCode, 1);
    --simulateDepth_;
    return result == 1;
}

bool DdDriver::SendUp(const KeyDescriptor& key, InputSource src) {
    if (!IsReady()) return false;
    if (simulateDepth_.load() > 3) return false;

    if (src == InputSource::Injected) {
        MarkInjected(key);
    }

    if (key.device == InputDevice::Mouse) {
        const auto mouseCodes = ToMouseCodes(key.code);
        if (!mouseCodes.has_value()) return false;
        ++simulateDepth_;
        const int result = ddBtn_(mouseCodes->second);
        RegisterRelease(key);
        --simulateDepth_;
        return result == 1;
    }

    const int ddCode = ResolveKeyboardDdCode(ddToDc_, key.code);
    if (ddCode <= 0) return false;
    ++simulateDepth_;
    const int result = ddKey_(ddCode, 2);
    RegisterRelease(key);
    --simulateDepth_;
    return result == 1;
}

void DdDriver::ReleaseAll() {
    if (!IsReady()) {
        return;
    }

    std::lock_guard lock(mutex_);
    for (const auto& [encoded, count] : pressedKeys_) {
        if (count <= 0) {
            continue;
        }

        const bool isMouse = (encoded >> 32) != 0;
        const std::uint32_t code = static_cast<std::uint32_t>(encoded & 0xFFFFFFFFull);

        if (isMouse) {
            if (const auto mouseCodes = ToMouseCodes(code); mouseCodes.has_value()) {
                ddBtn_(mouseCodes->second);
            }
            continue;
        }

        const int ddCode = ResolveKeyboardDdCode(ddToDc_, code);
        if (ddCode > 0) {
            ddKey_(ddCode, 2);
        }
    }
    pressedKeys_.clear();
}

void DdDriver::ClearAllStates() {
    std::lock_guard lock(mutex_);
    pressedKeys_.clear();
    selfInjected_.clear();
    simulateDepth_.store(0);
}

bool DdDriver::SendKeyboardTap(std::uint32_t vk) {
    const int ddCode = ResolveKeyboardDdCode(ddToDc_, vk);
    if (ddCode <= 0) {
        return false;
    }

    const KeyDescriptor key{InputDevice::Keyboard, vk};
    ++simulateDepth_;
    RegisterPress(key);
    const int downResult = ddKey_(ddCode, 1);
    const int upResult = ddKey_(ddCode, 2);
    RegisterRelease(key);
    --simulateDepth_;
    return downResult == 1 && upResult == 1;
}

bool DdDriver::SendMouseTap(std::uint32_t vk) {
    const auto mouseCodes = ToMouseCodes(vk);
    if (!mouseCodes.has_value()) {
        return false;
    }

    const KeyDescriptor key{InputDevice::Mouse, vk};
    ++simulateDepth_;
    RegisterPress(key);
    const int downResult = ddBtn_(mouseCodes->first);
    const int upResult = ddBtn_(mouseCodes->second);
    RegisterRelease(key);
    --simulateDepth_;
    return downResult == 1 && upResult == 1;
}

void DdDriver::RegisterPress(const KeyDescriptor& key) {
    std::lock_guard lock(mutex_);
    ++pressedKeys_[EncodeKey(key)];
}

void DdDriver::RegisterRelease(const KeyDescriptor& key) {
    std::lock_guard lock(mutex_);
    const auto encoded = EncodeKey(key);
    const auto it = pressedKeys_.find(encoded);
    if (it == pressedKeys_.end()) {
        return;
    }
    if (--it->second <= 0) {
        pressedKeys_.erase(it);
    }
}

void DdDriver::MarkInjected(const KeyDescriptor& key) {
    std::lock_guard lock(mutex_);
    selfInjected_.insert(EncodeKey(key));
}

bool DdDriver::IsSelfInjected(const KeyDescriptor& key) const {
    std::lock_guard lock(mutex_);
    return selfInjected_.count(EncodeKey(key)) > 0;
}
