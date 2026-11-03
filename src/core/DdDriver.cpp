#include "DdDriver.h"

#include "DiagLog.h"

#include <SetupAPI.h>
#include <hidsdi.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <cwctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace {

constexpr auto kHidInitGuardTailDelay = std::chrono::milliseconds(100);
constexpr int kHidInitCornerTolerancePx = 3;
constexpr wchar_t kDdxoftHidDeviceToken[] = L"ddhid63340";
constexpr DWORD kDdxoftHidMouseOutputReportBytes = 65;
constexpr std::uint8_t kDdxoftHidOutputReportId = 0x40;
constexpr std::uint8_t kDdxoftHidMouseCommand = 0x09;
constexpr std::uint8_t kDdxoftHidRelativeMouseReportId = 0x04;
constexpr ULONGLONG kSelfInjectedTtlMs = 80;
constexpr int kMaxPendingInjectedEvents = 8;

bool IsInjectedSource(InputSource src) {
    return src == InputSource::Injected ||
        src == InputSource::Repeater ||
        src == InputSource::Macro;
}

bool IsNearHidInitCorner(const POINT& point) {
    const int virtualLeft = ::GetSystemMetrics(SM_XVIRTUALSCREEN);
    const int virtualTop = ::GetSystemMetrics(SM_YVIRTUALSCREEN);
    const bool nearVirtualTopLeft =
        std::abs(point.x - virtualLeft) <= kHidInitCornerTolerancePx &&
        std::abs(point.y - virtualTop) <= kHidInitCornerTolerancePx;
    const bool nearPrimaryTopLeft =
        std::abs(point.x) <= kHidInitCornerTolerancePx &&
        std::abs(point.y) <= kHidInitCornerTolerancePx;
    return nearVirtualTopLeft || nearPrimaryTopLeft;
}

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

LRESULT CALLBACK BlockHidInitMouseProc(int code, WPARAM wParam, LPARAM lParam) {
    if (code == HC_ACTION && wParam == WM_MOUSEMOVE) {
        const auto* event = reinterpret_cast<const MSLLHOOKSTRUCT*>(lParam);
        if (event != nullptr && IsNearHidInitCorner(event->pt)) {
            return 1;
        }
    }
    return ::CallNextHookEx(nullptr, code, wParam, lParam);
}

class MouseSettingsGuard {
public:
    MouseSettingsGuard() {
        mouseParamsCaptured_ = ::SystemParametersInfoW(SPI_GETMOUSE, 0, mouseParams_.data(), 0) != FALSE;
        mouseSpeedCaptured_ = ::SystemParametersInfoW(SPI_GETMOUSESPEED, 0, &mouseSpeed_, 0) != FALSE;
    }

    ~MouseSettingsGuard() {
        if (mouseParamsCaptured_) {
            std::array<int, 3> current{};
            if (::SystemParametersInfoW(SPI_GETMOUSE, 0, current.data(), 0) != FALSE &&
                current != mouseParams_) {
                ::SystemParametersInfoW(SPI_SETMOUSE, 0, mouseParams_.data(), 0);
            }
        }

        if (mouseSpeedCaptured_) {
            int currentSpeed = 0;
            if (::SystemParametersInfoW(SPI_GETMOUSESPEED, 0, &currentSpeed, 0) != FALSE &&
                currentSpeed != mouseSpeed_) {
                ::SystemParametersInfoW(SPI_SETMOUSESPEED, 0, reinterpret_cast<void*>(static_cast<INT_PTR>(mouseSpeed_)), 0);
            }
        }
    }

    MouseSettingsGuard(const MouseSettingsGuard&) = delete;
    MouseSettingsGuard& operator=(const MouseSettingsGuard&) = delete;

private:
    std::array<int, 3> mouseParams_{};
    int mouseSpeed_ = 0;
    bool mouseParamsCaptured_ = false;
    bool mouseSpeedCaptured_ = false;
};

class HidInitGuard {
public:
    HidInitGuard() {
        stopEvent_ = ::CreateEventW(nullptr, TRUE, FALSE, nullptr);
        readyEvent_ = ::CreateEventW(nullptr, TRUE, FALSE, nullptr);
        if (stopEvent_ != nullptr && readyEvent_ != nullptr) {
            hookThread_ = std::thread([this]() {
                const HHOOK keyboardHook = ::SetWindowsHookExW(
                    WH_KEYBOARD_LL,
                    BlockWinKeyProc,
                    ::GetModuleHandleW(nullptr),
                    0);
                const HHOOK mouseHook = ::SetWindowsHookExW(
                    WH_MOUSE_LL,
                    BlockHidInitMouseProc,
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

                if (mouseHook != nullptr) {
                    ::UnhookWindowsHookEx(mouseHook);
                }
                if (keyboardHook != nullptr) {
                    ::UnhookWindowsHookEx(keyboardHook);
                }
            });
            ::WaitForSingleObject(readyEvent_, 1000);
        }

        cursorCaptured_ = ::GetCursorPos(&oldPos_) != FALSE;
    }

    ~HidInitGuard() {
        // Keep the input guard briefly after DD_btn(0) returns because the HID runtime may emit delayed input.
        std::this_thread::sleep_for(kHidInitGuardTailDelay);
        RestoreCursorIfHidJumped();
        if (stopEvent_ != nullptr) {
            ::SetEvent(stopEvent_);
        }
        if (hookThread_.joinable()) {
            hookThread_.join();
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
    void RestoreCursorIfHidJumped() const {
        if (!cursorCaptured_ || IsNearHidInitCorner(oldPos_)) {
            return;
        }

        POINT current{};
        if (::GetCursorPos(&current) == FALSE) {
            return;
        }
        if (IsNearHidInitCorner(current)) {
            ::SetCursorPos(oldPos_.x, oldPos_.y);
        }
    }

    POINT oldPos_{};
    HANDLE readyEvent_ = nullptr;
    HANDLE stopEvent_ = nullptr;
    std::thread hookThread_;
    bool cursorCaptured_ = false;
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

std::optional<std::uint8_t> ToHidMouseButtonBit(std::uint32_t vk) {
    switch (vk) {
    case VK_LBUTTON:
        return 0x01;
    case VK_RBUTTON:
        return 0x02;
    case VK_MBUTTON:
        return 0x04;
    case VK_XBUTTON1:
        return 0x08;
    case VK_XBUTTON2:
        return 0x10;
    default:
        return std::nullopt;
    }
}

std::wstring ToLower(std::wstring value) {
    std::transform(value.begin(), value.end(), value.begin(), [](wchar_t ch) {
        return static_cast<wchar_t>(std::towlower(ch));
    });
    return value;
}

bool IsDdxoftHidDevicePath(const wchar_t* devicePath) {
    if (devicePath == nullptr || *devicePath == L'\0') {
        return false;
    }
    return ToLower(devicePath).find(kDdxoftHidDeviceToken) != std::wstring::npos;
}

bool HasDdxoftHidOutputReport(HANDLE handle) {
    PHIDP_PREPARSED_DATA preparsedData = nullptr;
    if (!::HidD_GetPreparsedData(handle, &preparsedData)) {
        return false;
    }

    HIDP_CAPS caps{};
    const NTSTATUS capsStatus = ::HidP_GetCaps(preparsedData, &caps);
    ::HidD_FreePreparsedData(preparsedData);
    return capsStatus == HIDP_STATUS_SUCCESS &&
        caps.OutputReportByteLength >= kDdxoftHidMouseOutputReportBytes;
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
    return LoadFrom(dllPath, RuntimeKind::Legacy);
}

bool DdDriver::LoadHidFrom(const std::wstring& dllPath) {
    return LoadFrom(dllPath, RuntimeKind::Hid);
}

bool DdDriver::LoadFrom(const std::wstring& dllPath, RuntimeKind runtimeKind) {
    // DD DLL initialization can change mouse acceleration inside LoadLibraryW.
    MouseSettingsGuard mouseSettingsGuard;
    Unload();

    module_ = ::LoadLibraryW(dllPath.c_str());
    if (module_ == nullptr) {
        const DWORD errorCode = ::GetLastError();
        lastError_ = L"Unable to load DD DLL: " + dllPath + L"; win32=" + FormatWin32Error(errorCode);
        return false;
    }

    ddBtn_ = reinterpret_cast<DD_btn_t>(::GetProcAddress(module_, "DD_btn"));
    ddKey_ = reinterpret_cast<DD_key_t>(::GetProcAddress(module_, "DD_key"));
    ddWheel_ = reinterpret_cast<DD_btn_t>(::GetProcAddress(module_, "DD_whl"));
    ddToDc_ = reinterpret_cast<DD_todc_t>(::GetProcAddress(module_, "DD_todc"));
    if (ddBtn_ == nullptr || ddKey_ == nullptr || ddToDc_ == nullptr) {
        lastError_ = L"DD DLL is missing required exports";
        Unload();
        return false;
    }

    // 可选的鼠标移动导出，大小写两种命名都尝试。
    ddMovR_ = reinterpret_cast<DD_move_t>(::GetProcAddress(module_, "DD_movR"));
    if (ddMovR_ == nullptr) {
        ddMovR_ = reinterpret_cast<DD_move_t>(::GetProcAddress(module_, "dd_movR"));
    }
    ddMov_ = reinterpret_cast<DD_move_t>(::GetProcAddress(module_, "DD_mov"));
    if (ddMov_ == nullptr) {
        ddMov_ = reinterpret_cast<DD_move_t>(::GetProcAddress(module_, "dd_mov"));
    }

    int initResult = [this, runtimeKind]() {
        if (runtimeKind == RuntimeKind::Hid) {
            HidInitGuard guard;
            return ddBtn_(0);
        }
        return ddBtn_(0);
    }();
    if (initResult != 1) {
        std::wstring bindError;
        if (runtimeKind == RuntimeKind::Legacy && TryBindInfDeviceHandles(dllPath, &bindError)) {
            initResult = ddBtn_(0);
        }
        if (initResult != 1) {
            lastError_ = runtimeKind == RuntimeKind::Hid
                ? L"Please install the DD HID driver package first"
                : L"Please install the DD INF driver package first; DD_btn(0)=" + std::to_wstring(initResult);
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

    runtimeKind_ = runtimeKind;
    lastError_.clear();
    return true;
}

bool DdDriver::LoadLegacyFromDirectory(const std::wstring& directoryPath) {
    const std::filesystem::path directory(directoryPath);
    const std::filesystem::path driverDir = directory / L"driver";

#if defined(_WIN64)
    constexpr std::wstring_view primaryDllName = L"DD64.dll";
    constexpr std::wstring_view secondaryDllName = L"DD32.dll";
#else
    constexpr std::wstring_view primaryDllName = L"DD32.dll";
    constexpr std::wstring_view secondaryDllName = L"DD64.dll";
#endif

    const std::vector<std::filesystem::path> candidates = {
        driverDir / primaryDllName,
        driverDir / secondaryDllName,
        directory / primaryDllName,
        directory / secondaryDllName,
    };

    std::wstring firstMeaningfulError;
    for (const auto& candidate : candidates) {
        if (LoadFrom(candidate.wstring(), RuntimeKind::Legacy)) {
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
    // DLL teardown can restore stale settings after the user changed them.
    MouseSettingsGuard mouseSettingsGuard;
    {
        std::lock_guard lock(hidMouseMutex_);
        CloseHidMouseHandleLocked();
        hidMouseButtonState_ = 0;
    }
    CloseInjectedInfHandles();
    if (module_ != nullptr) {
        ::FreeLibrary(module_);
        module_ = nullptr;
    }
    ddBtn_ = nullptr;
    ddKey_ = nullptr;
    ddToDc_ = nullptr;
    ddWheel_ = nullptr;
    ddMov_ = nullptr;
    ddMovR_ = nullptr;
    runtimeKind_ = RuntimeKind::Unknown;
}

void DdDriver::SwapWith(DdDriver& other) {
    if (this == &other) {
        return;
    }

    std::scoped_lock lock(mutex_, other.mutex_, hidMouseMutex_, other.hidMouseMutex_);
    std::swap(module_, other.module_);
    std::swap(ddBtn_, other.ddBtn_);
    std::swap(ddKey_, other.ddKey_);
    std::swap(ddToDc_, other.ddToDc_);
    std::swap(ddWheel_, other.ddWheel_);
    std::swap(ddMov_, other.ddMov_);
    std::swap(ddMovR_, other.ddMovR_);
    std::swap(runtimeKind_, other.runtimeKind_);
    std::swap(infKeyboardHandle_, other.infKeyboardHandle_);
    std::swap(infMouseHandle_, other.infMouseHandle_);
    std::swap(infHandlesInjected_, other.infHandlesInjected_);
    lastError_.swap(other.lastError_);

    const int thisDepth = simulateDepth_.load(std::memory_order_relaxed);
    simulateDepth_.store(other.simulateDepth_.load(std::memory_order_relaxed), std::memory_order_relaxed);
    other.simulateDepth_.store(thisDepth, std::memory_order_relaxed);

    pressedKeys_.swap(other.pressedKeys_);
    selfInjected_.swap(other.selfInjected_);
    std::swap(hidMouseHandle_, other.hidMouseHandle_);
    std::swap(hidMouseButtonState_, other.hidMouseButtonState_);
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
    if (!SendDown(key, src)) return false;
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    return SendUp(key, src);
}

bool DdDriver::SendDown(const KeyDescriptor& key, InputSource src) {
    std::lock_guard ioLock(ioMutex_);
    if (!IsReady()) return false;
    if (simulateDepth_.load() > 3) return false;

    const bool trackInjected = IsInjectedSource(src);
    if (trackInjected) MarkInjected(key, true);

    if (key.device == InputDevice::Mouse) {
        ++simulateDepth_;
        const bool result = SendMouseDown(key.code);
        if (result) RegisterPress(key);
        --simulateDepth_;
        NoteMouseInjectResult(key, true, result);
        if (!result && trackInjected) RollbackInjected(key, true);
        return result;
    }

    const int ddCode = ResolveKeyboardDdCode(ddToDc_, key.code);
    if (ddCode <= 0) {
        if (trackInjected) RollbackInjected(key, true);
        return false;
    }
    ++simulateDepth_;
    const int result = ddKey_(ddCode, 1);
    if (result == 1) RegisterPress(key);
    --simulateDepth_;
    if (result != 1 && trackInjected) RollbackInjected(key, true);
    return result == 1;
}

bool DdDriver::SendUp(const KeyDescriptor& key, InputSource src) {
    std::lock_guard ioLock(ioMutex_);
    if (!IsReady()) return false;
    if (simulateDepth_.load() > 3) return false;

    const bool trackInjected = IsInjectedSource(src);
    if (trackInjected) MarkInjected(key, false);

    if (key.device == InputDevice::Mouse) {
        ++simulateDepth_;
        const bool result = SendMouseUp(key.code);
        if (result) RegisterRelease(key);
        --simulateDepth_;
        NoteMouseInjectResult(key, false, result);
        if (!result && trackInjected) RollbackInjected(key, false);
        return result;
    }

    const int ddCode = ResolveKeyboardDdCode(ddToDc_, key.code);
    if (ddCode <= 0) {
        if (trackInjected) RollbackInjected(key, false);
        return false;
    }
    ++simulateDepth_;
    const int result = ddKey_(ddCode, 2);
    if (result == 1) RegisterRelease(key);
    --simulateDepth_;
    if (result != 1 && trackInjected) RollbackInjected(key, false);
    return result == 1;
}

void DdDriver::ReleaseAll() {
    std::lock_guard ioLock(ioMutex_);
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
            SendMouseUp(code);
            continue;
        }

        const int ddCode = ResolveKeyboardDdCode(ddToDc_, code);
        if (ddCode > 0) {
            ddKey_(ddCode, 2);
        }
    }
    pressedKeys_.clear();
    {
        std::lock_guard hidLock(hidMouseMutex_);
        hidMouseButtonState_ = 0;
        if (hidMouseHandle_ != INVALID_HANDLE_VALUE) {
            WriteHidMouseReportLocked();
        }
    }
}

void DdDriver::ClearAllStates() {
    std::lock_guard ioLock(ioMutex_);
    std::lock_guard lock(mutex_);
    pressedKeys_.clear();
    selfInjected_.clear();
    simulateDepth_.store(0);
    mouseInjectFailDiag_.clear();
}

bool DdDriver::SendKeyboardTap(std::uint32_t vk) {
    const int ddCode = ResolveKeyboardDdCode(ddToDc_, vk);
    if (ddCode <= 0) {
        return false;
    }

    const KeyDescriptor key{InputDevice::Keyboard, vk};
    ++simulateDepth_;
    const int downResult = ddKey_(ddCode, 1);
    if (downResult == 1) RegisterPress(key);
    const int upResult = ddKey_(ddCode, 2);
    if (upResult == 1) RegisterRelease(key);
    --simulateDepth_;
    return downResult == 1 && upResult == 1;
}

bool DdDriver::SendMouseTap(std::uint32_t vk) {
    const KeyDescriptor key{InputDevice::Mouse, vk};
    ++simulateDepth_;
    const bool downResult = SendMouseDown(vk);
    if (downResult) RegisterPress(key);
    const bool upResult = SendMouseUp(vk);
    if (upResult) RegisterRelease(key);
    --simulateDepth_;
    return downResult && upResult;
}

bool DdDriver::SendMouseDown(std::uint32_t vk) {
    if (runtimeKind_ == RuntimeKind::Hid) {
        if (SendHidMouseButton(vk, true)) {
            return true;
        }
        if (vk == VK_XBUTTON1 || vk == VK_XBUTTON2) {
            return false;
        }
    }

    const auto mouseCodes = ToMouseCodes(vk);
    if (!mouseCodes.has_value()) {
        return false;
    }
    return ddBtn_(mouseCodes->first) == 1;
}

bool DdDriver::SendMouseUp(std::uint32_t vk) {
    if (runtimeKind_ == RuntimeKind::Hid) {
        if (SendHidMouseButton(vk, false)) {
            return true;
        }
        if (vk == VK_XBUTTON1 || vk == VK_XBUTTON2) {
            return false;
        }
    }

    const auto mouseCodes = ToMouseCodes(vk);
    if (!mouseCodes.has_value()) {
        return false;
    }
    return ddBtn_(mouseCodes->second) == 1;
}

bool DdDriver::SendMouseMove(int deltaX, int deltaY, InputSource) {
    std::lock_guard ioLock(ioMutex_);
    if (!IsReady()) return false;

    if (runtimeKind_ == RuntimeKind::Hid) {
        // ddxoft HID 相对鼠标报文，位移按有符号字节分块发送。
        std::lock_guard hidLock(hidMouseMutex_);
        if (!EnsureHidMouseHandleLocked()) {
            return false;
        }
        while (deltaX != 0 || deltaY != 0) {
            const auto step = [](int value) -> signed char {
                return static_cast<signed char>(std::clamp(value, -127, 127));
            };
            const int stepX = step(deltaX);
            const int stepY = step(deltaY);
            if (!WriteHidMouseReportLocked(stepX, stepY)) {
                CloseHidMouseHandleLocked();
                if (!EnsureHidMouseHandleLocked() || !WriteHidMouseReportLocked(stepX, stepY)) {
                    return false;
                }
            }
            deltaX -= stepX;
            deltaY -= stepY;
        }
        return true;
    }

    if (ddMovR_ != nullptr) {
        return ddMovR_(deltaX, deltaY) == 1;
    }
    if (ddMov_ != nullptr) {
        POINT current{};
        if (!::GetCursorPos(&current)) {
            return false;
        }
        return ddMov_(static_cast<int>(current.x) + deltaX, static_cast<int>(current.y) + deltaY) == 1;
    }
    return false;
}

bool DdDriver::SupportsMouseMove() const {
    if (runtimeKind_ == RuntimeKind::Hid) {
        return true;  // 句柄按需打开
    }
    return ddMov_ != nullptr || ddMovR_ != nullptr;
}

bool DdDriver::SendHidMouseButton(std::uint32_t vk, bool down) {
    const auto buttonBit = ToHidMouseButtonBit(vk);
    if (!buttonBit.has_value()) {
        return false;
    }

    std::lock_guard lock(hidMouseMutex_);
    if (!EnsureHidMouseHandleLocked()) {
        return false;
    }

    const std::uint8_t oldState = hidMouseButtonState_;
    if (down) {
        hidMouseButtonState_ = static_cast<std::uint8_t>(hidMouseButtonState_ | *buttonBit);
    } else {
        hidMouseButtonState_ = static_cast<std::uint8_t>(hidMouseButtonState_ & ~*buttonBit);
    }

    if (WriteHidMouseReportLocked()) {
        return true;
    }

    CloseHidMouseHandleLocked();
    if (!EnsureHidMouseHandleLocked()) {
        hidMouseButtonState_ = oldState;
        return false;
    }
    if (WriteHidMouseReportLocked()) {
        return true;
    }

    hidMouseButtonState_ = oldState;
    return false;
}

bool DdDriver::EnsureHidMouseHandleLocked() {
    if (hidMouseHandle_ != INVALID_HANDLE_VALUE) {
        return true;
    }

    GUID hidGuid{};
    ::HidD_GetHidGuid(&hidGuid);

    HDEVINFO deviceInfo = ::SetupDiGetClassDevsW(
        &hidGuid,
        nullptr,
        nullptr,
        DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (deviceInfo == INVALID_HANDLE_VALUE) {
        return false;
    }

    bool opened = false;
    int matchedInterfaces = 0;
    int capsRejected = 0;
    DWORD lastCreateError = 0;
    for (DWORD index = 0; !opened; ++index) {
        SP_DEVICE_INTERFACE_DATA interfaceData{};
        interfaceData.cbSize = sizeof(interfaceData);
        if (!::SetupDiEnumDeviceInterfaces(deviceInfo, nullptr, &hidGuid, index, &interfaceData)) {
            break;
        }

        DWORD requiredSize = 0;
        ::SetupDiGetDeviceInterfaceDetailW(
            deviceInfo,
            &interfaceData,
            nullptr,
            0,
            &requiredSize,
            nullptr);
        if (requiredSize == 0) {
            continue;
        }

        std::vector<BYTE> detailBuffer(requiredSize);
        auto* detailData = reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W*>(detailBuffer.data());
        detailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);
        if (!::SetupDiGetDeviceInterfaceDetailW(
                deviceInfo,
                &interfaceData,
                detailData,
                requiredSize,
                nullptr,
                nullptr)) {
            continue;
        }

        if (!IsDdxoftHidDevicePath(detailData->DevicePath)) {
            continue;
        }
        ++matchedInterfaces;

        HANDLE handle = ::CreateFileW(
            detailData->DevicePath,
            GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            nullptr,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            nullptr);
        if (handle == INVALID_HANDLE_VALUE) {
            lastCreateError = ::GetLastError();
            continue;
        }

        if (!HasDdxoftHidOutputReport(handle)) {
            ++capsRejected;
            ::CloseHandle(handle);
            continue;
        }

        ::HidD_SetNumInputBuffers(handle, 64);
        hidMouseHandle_ = handle;
        opened = true;
    }

    ::SetupDiDestroyDeviceInfoList(deviceInfo);
    if (opened) {
        hidMouseOpenDiagLogged_ = false;
        return true;
    }

    if (!hidMouseOpenDiagLogged_) {
        hidMouseOpenDiagLogged_ = true;
        char line[160] = {};
        std::snprintf(line, sizeof(line),
            "HID mouse handle open failed: matched=%d capsRejected=%d createError=%lu",
            matchedInterfaces, capsRejected, lastCreateError);
        DiagLog::Write(line);
    }
    return false;
}

bool DdDriver::WriteHidMouseReportLocked(int deltaX, int deltaY) {
    if (hidMouseHandle_ == INVALID_HANDLE_VALUE) {
        return false;
    }

    std::array<BYTE, kDdxoftHidMouseOutputReportBytes> report{};
    report[0] = kDdxoftHidOutputReportId;
    report[1] = kDdxoftHidMouseCommand;
    report[2] = kDdxoftHidRelativeMouseReportId;
    report[3] = hidMouseButtonState_;
    // 报文第 4/5 字节是相对位移（有符号字节），单次最多 127 像素。
    report[4] = static_cast<BYTE>(static_cast<signed char>(deltaX));
    report[5] = static_cast<BYTE>(static_cast<signed char>(deltaY));

    DWORD written = 0;
    return ::WriteFile(
        hidMouseHandle_,
        report.data(),
        static_cast<DWORD>(report.size()),
        &written,
        nullptr) != FALSE &&
        written == report.size();
}

void DdDriver::CloseHidMouseHandleLocked() {
    if (hidMouseHandle_ != INVALID_HANDLE_VALUE) {
        ::CloseHandle(hidMouseHandle_);
        hidMouseHandle_ = INVALID_HANDLE_VALUE;
    }
}

void DdDriver::NoteMouseInjectResult(const KeyDescriptor& key, bool down, bool ok) {
    const auto diagKey = (EncodeKey(key) << 1) | (down ? 0u : 1u);
    if (ok) {
        mouseInjectFailDiag_.erase(diagKey);
        return;
    }
    if (mouseInjectFailDiag_.count(diagKey) > 0) {
        return;
    }
    mouseInjectFailDiag_.insert(diagKey);

    char line[160] = {};
    std::snprintf(line, sizeof(line), "mouse %s inject failed: vk=%u runtime=%s",
        down ? "down" : "up",
        key.code,
        runtimeKind_ == RuntimeKind::Hid ? "hid" : "dd");
    DiagLog::Write(line);
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

void DdDriver::MarkInjected(const KeyDescriptor& key, bool pressed, int eventCount) {
    if (eventCount <= 0) {
        return;
    }

    std::lock_guard lock(mutex_);
    auto& state = selfInjected_[EncodeKey(key)];
    int& count = pressed ? state.downCount : state.upCount;
    ULONGLONG& lastTick = pressed ? state.lastDownTick : state.lastUpTick;
    count = std::min(kMaxPendingInjectedEvents, count + eventCount);
    lastTick = ::GetTickCount64();
}

void DdDriver::RollbackInjected(const KeyDescriptor& key, bool pressed, int eventCount) {
    if (eventCount <= 0) {
        return;
    }

    std::lock_guard lock(mutex_);
    const auto encoded = EncodeKey(key);
    auto it = selfInjected_.find(encoded);
    if (it == selfInjected_.end()) {
        return;
    }

    int& count = pressed ? it->second.downCount : it->second.upCount;
    count = std::max(0, count - eventCount);
    if (it->second.downCount == 0 && it->second.upCount == 0) {
        selfInjected_.erase(it);
    }
}

bool DdDriver::ConsumeSelfInjected(const KeyDescriptor& key, bool pressed) {
    std::lock_guard lock(mutex_);
    const auto encoded = EncodeKey(key);
    auto it = selfInjected_.find(encoded);
    if (it == selfInjected_.end()) {
        return false;
    }

    const ULONGLONG now = ::GetTickCount64();
    if (it->second.downCount > 0 && now - it->second.lastDownTick > kSelfInjectedTtlMs) {
        it->second.downCount = 0;
    }
    if (it->second.upCount > 0 && now - it->second.lastUpTick > kSelfInjectedTtlMs) {
        it->second.upCount = 0;
    }

    int& count = pressed ? it->second.downCount : it->second.upCount;
    if (count <= 0) {
        if (it->second.downCount == 0 && it->second.upCount == 0) {
            selfInjected_.erase(it);
        }
        return false;
    }

    --count;
    if (it->second.downCount == 0 && it->second.upCount == 0) {
        selfInjected_.erase(it);
    }
    return true;
}

bool DdDriver::IsSelfInjected(const KeyDescriptor& key) const {
    std::lock_guard lock(mutex_);
    const auto it = selfInjected_.find(EncodeKey(key));
    if (it == selfInjected_.end()) {
        return false;
    }
    const ULONGLONG now = ::GetTickCount64();
    return (it->second.downCount > 0 && now - it->second.lastDownTick <= kSelfInjectedTtlMs) ||
        (it->second.upCount > 0 && now - it->second.lastUpTick <= kSelfInjectedTtlMs);
}

bool DdDriver::SendMouseWheel(int delta, InputSource) {
    std::lock_guard ioLock(ioMutex_);
    if (!IsReady() || !ddWheel_ || delta == 0 || delta % 120 != 0) return false;
    for (int count = std::abs(delta / 120); count > 0; --count) {
        if (ddWheel_(delta > 0 ? 1 : 2) != 1) return false;
    }
    return true;
}
bool DdDriver::SupportsMouseWheel() const { return ddWheel_ != nullptr; }
