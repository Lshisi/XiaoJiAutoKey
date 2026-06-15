#include "DdDriver.h"

#include <chrono>
#include <filesystem>
#include <sstream>
#include <string_view>
#include <thread>

namespace {

LRESULT CALLBACK BlockWinKeyProc(int code, WPARAM wParam, LPARAM lParam) {
    if (code == HC_ACTION) {
        const auto* event = reinterpret_cast<const KBDLLHOOKSTRUCT*>(lParam);
        if (event != nullptr && (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN ||
                wParam == WM_KEYUP || wParam == WM_SYSKEYUP) &&
            (event->vkCode == VK_LWIN || event->vkCode == VK_RWIN)) {
            return 1;
        }
    }
    return ::CallNextHookEx(nullptr, code, wParam, lParam);
}

class HidInitGuard {
public:
    HidInitGuard() {
        stopEvent_ = ::CreateEventW(nullptr, TRUE, FALSE, nullptr);
        readyEvent_ = ::CreateEventW(nullptr, TRUE, FALSE, nullptr);
        if (stopEvent_ != nullptr && readyEvent_ != nullptr) {
            hookThread_ = std::thread([this]() {
                const HHOOK hook = ::SetWindowsHookExW(
                    WH_KEYBOARD_LL,
                    BlockWinKeyProc,
                    ::GetModuleHandleW(nullptr),
                    0);
                ::SetEvent(readyEvent_);

                MSG message{};
                while (true) {
                    const DWORD waitResult = ::MsgWaitForMultipleObjects(
                        1,
                        &stopEvent_,
                        FALSE,
                        INFINITE,
                        QS_ALLINPUT);
                    if (waitResult == WAIT_OBJECT_0) {
                        break;
                    }
                    while (::PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
                        ::TranslateMessage(&message);
                        ::DispatchMessageW(&message);
                    }
                }

                if (hook != nullptr) {
                    ::UnhookWindowsHookEx(hook);
                }
            });
            ::WaitForSingleObject(readyEvent_, 1000);
        }

        cursorCaptured_ = ::GetCursorPos(&oldPos_) != FALSE;
        if (cursorCaptured_) {
            RECT rect{oldPos_.x, oldPos_.y, oldPos_.x + 1, oldPos_.y + 1};
            cursorClipped_ = ::ClipCursor(&rect) != FALSE;
        }
    }

    ~HidInitGuard() {
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        if (stopEvent_ != nullptr) {
            ::SetEvent(stopEvent_);
        }
        if (hookThread_.joinable()) {
            hookThread_.join();
        }
        if (cursorClipped_) {
            ::ClipCursor(nullptr);
        }
        if (cursorCaptured_) {
            ::SetCursorPos(oldPos_.x, oldPos_.y);
        }
        if (readyEvent_ != nullptr) {
            ::CloseHandle(readyEvent_);
        }
        if (stopEvent_ != nullptr) {
            ::CloseHandle(stopEvent_);
        }
    }

    HidInitGuard(const HidInitGuard&) = delete;
    HidInitGuard& operator=(const HidInitGuard&) = delete;

private:
    POINT oldPos_{};
    HANDLE readyEvent_ = nullptr;
    HANDLE stopEvent_ = nullptr;
    std::thread hookThread_;
    bool cursorCaptured_ = false;
    bool cursorClipped_ = false;
};

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
    return LoadFrom(dllPath, RuntimeKind::Legacy);
}

bool DdDriver::LoadHidFrom(const std::wstring& dllPath) {
    return LoadFrom(dllPath, RuntimeKind::Hid);
}

bool DdDriver::LoadFrom(const std::wstring& dllPath, RuntimeKind runtimeKind) {
    Unload();

    module_ = ::LoadLibraryW(dllPath.c_str());
    if (module_ == nullptr) {
        lastError_ = L"Unable to load DD DLL: " + dllPath;
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

    const int initResult = [this, runtimeKind]() {
        if (runtimeKind == RuntimeKind::Hid) {
            HidInitGuard guard;
            return ddBtn_(0);
        }
        return ddBtn_(0);
    }();
    if (initResult != 1) {
        lastError_ = runtimeKind == RuntimeKind::Hid
            ? L"Please install the DD HID driver package first"
            : L"Please install the DD INF driver package first";
        Unload();
        return false;
    }

    runtimeKind_ = runtimeKind;
    lastError_.clear();
    return true;
}

bool DdDriver::LoadFromDirectory(const std::wstring& directoryPath, bool allowHid) {
    const std::filesystem::path directory(directoryPath);
    const std::filesystem::path driverDir = directory / L"driver";
    const std::filesystem::path hidDll = driverDir / L"hid" / L"ddhid.63340.dll";

#if defined(_WIN64)
    constexpr std::wstring_view primaryDllName = L"DD64.dll";
    constexpr std::wstring_view secondaryDllName = L"DD32.dll";
#else
    constexpr std::wstring_view primaryDllName = L"DD32.dll";
    constexpr std::wstring_view secondaryDllName = L"DD64.dll";
#endif

    if (allowHid && LoadHidFrom(hidDll.wstring())) {
        return true;
    }

    if (LoadFrom((driverDir / primaryDllName).wstring(), RuntimeKind::Legacy)) {
        return true;
    }
    if (LoadFrom((driverDir / secondaryDllName).wstring(), RuntimeKind::Legacy)) {
        return true;
    }

    if (LoadFrom((directory / primaryDllName).wstring(), RuntimeKind::Legacy)) {
        return true;
    }
    if (LoadFrom((directory / secondaryDllName).wstring(), RuntimeKind::Legacy)) {
        return true;
    }

    if (lastError_ != L"Please install the DD HID driver package first" &&
        lastError_ != L"Please install the DD INF driver package first") {
        lastError_ = L"Unable to load DD HID DLL, DD32.dll, or DD64.dll";
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
    runtimeKind_ = RuntimeKind::Unknown;
}

bool DdDriver::IsReady() const noexcept {
    return module_ != nullptr && ddBtn_ != nullptr && ddKey_ != nullptr && ddToDc_ != nullptr;
}

std::wstring DdDriver::GetLastError() const {
    return lastError_;
}

DdDriver::RuntimeKind DdDriver::GetRuntimeKind() const noexcept {
    return runtimeKind_;
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
