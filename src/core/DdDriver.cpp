#include "DdDriver.h"

#include <filesystem>
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

    const int initResult = ddBtn_(0);
    if (initResult != 1) {
        lastError_ = L"Please install the DD INF driver package first; DD_btn(0)=" + std::to_wstring(initResult);
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
    if (module_ != nullptr) {
        ::FreeLibrary(module_);
        module_ = nullptr;
    }
    ddBtn_ = nullptr;
    ddKey_ = nullptr;
    ddToDc_ = nullptr;
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
