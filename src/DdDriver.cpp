#include "DdDriver.h"

#include <filesystem>

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

}  // namespace

DdDriver::~DdDriver() {
    ReleaseAll();
    Unload();
}

bool DdDriver::LoadFrom(const std::wstring& dllPath) {
    Unload();

    module_ = ::LoadLibraryW(dllPath.c_str());
    if (module_ == nullptr) {
        lastError_ = L"无法加载 dd.dll";
        return false;
    }

    ddBtn_ = reinterpret_cast<DD_btn_t>(::GetProcAddress(module_, "DD_btn"));
    ddKey_ = reinterpret_cast<DD_key_t>(::GetProcAddress(module_, "DD_key"));
    ddToDc_ = reinterpret_cast<DD_todc_t>(::GetProcAddress(module_, "DD_todc"));
    if (ddBtn_ == nullptr || ddKey_ == nullptr || ddToDc_ == nullptr) {
        lastError_ = L"dd.dll 缺少必要导出函数";
        Unload();
        return false;
    }

    if (ddBtn_(0) != 1) {
        lastError_ = L"请先安装 DD 驱动";
        Unload();
        return false;
    }

    lastError_.clear();
    return true;
}

bool DdDriver::LoadFromDirectory(const std::wstring& directoryPath) {
#if defined(_WIN64)
    constexpr std::wstring_view primaryDllName = L"DD64.dll";
    constexpr std::wstring_view secondaryDllName = L"DD32.dll";
#else
    constexpr std::wstring_view primaryDllName = L"DD32.dll";
    constexpr std::wstring_view secondaryDllName = L"DD64.dll";
#endif

    const std::filesystem::path directory(directoryPath);
    const std::filesystem::path driverDir = directory / L"driver";

    // 优先从 driver 子目录加载
    if (LoadFrom((driverDir / primaryDllName).wstring())) {
        return true;
    }
    if (LoadFrom((driverDir / secondaryDllName).wstring())) {
        return true;
    }

    // 回退：从当前目录加载
    if (LoadFrom((directory / primaryDllName).wstring())) {
        return true;
    }
    if (LoadFrom((directory / secondaryDllName).wstring())) {
        return true;
    }

    if (lastError_ == L"请先安装 DD 驱动") {
        // 保持原错误
    } else {
        lastError_ = L"无法加载 DD32.dll 或 DD64.dll";
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

    if (simulateDepth_.load() > 3)
        return false;

    // 标记为自我注入（仅当来源为Injected时）
    if (src == InputSource::Injected) {
        MarkInjected(key);
    }

    return key.device == InputDevice::Mouse ? SendMouseTap(key.code) : SendKeyboardTap(key.code);
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

        const int ddCode = ddToDc_(static_cast<int>(code));
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
    const int ddCode = ddToDc_(static_cast<int>(vk));
    if (ddCode <= 0) {
        return false;
    }

    const KeyDescriptor key{InputDevice::Keyboard, vk};
    ++simulateDepth_;
    RegisterPress(key);
    ddKey_(ddCode, 1);
    ddKey_(ddCode, 2);
    RegisterRelease(key);
    --simulateDepth_;
    return true;
}

bool DdDriver::SendMouseTap(std::uint32_t vk) {
    const auto mouseCodes = ToMouseCodes(vk);
    if (!mouseCodes.has_value()) {
        return false;
    }

    const KeyDescriptor key{InputDevice::Mouse, vk};
    ++simulateDepth_;
    RegisterPress(key);
    ddBtn_(mouseCodes->first);
    ddBtn_(mouseCodes->second);
    RegisterRelease(key);
    --simulateDepth_;
    return true;
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
